#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"

if [ -f ./.env ]; then
    # shellcheck disable=SC1091
    . ./.env
fi

REGISTRY_ADDR=${REGISTRY_ADDR:-localhost}
REGISTRY_PORT=${REGISTRY_PORT:-5000}
IMAGE_NAME=${IMAGE_NAME:-mpi-matrix}
SSH_PORT=${SSH_PORT:-2222}

DOCKER_BIN=${DOCKER_BIN:-docker}
if ! command -v "$DOCKER_BIN" >/dev/null 2>&1; then
    if [ -x "/mnt/c/Program Files/Docker/Docker/resources/bin/docker.exe" ]; then
        DOCKER_BIN="/mnt/c/Program Files/Docker/Docker/resources/bin/docker.exe"
    else
        echo "Error: no encuentro Docker. Define DOCKER_BIN o instala docker en PATH."
        exit 1
    fi
fi

docker_cmd() {
    "$DOCKER_BIN" "$@"
}

usage() {
    cat <<'EOF'
Uso:
  ./cluster.sh up [size=N]      Levanta cluster (N nodos totales, incluye master)
    ./cluster.sh scale [size=N]   Escala cluster sin recompilar imagen
  ./cluster.sh reload [size=N]  Recompila imagen y reinicia nodos
  ./cluster.sh down             Apaga cluster y registry
  ./cluster.sh list             Lista servicios
  ./cluster.sh exec <comando>   Ejecuta comando en el master como usuario mpi

Variables opcionales:
  DOCKER_BIN=/ruta/a/docker
EOF
}

ensure_ssh_keys() {
    mkdir -p ssh
    if [ ! -f ssh/id_rsa ] || [ ! -f ssh/id_rsa.pub ]; then
        ssh-keygen -f ssh/id_rsa -t rsa -N ''
    fi
}

build_and_push() {
    local image_ref="${REGISTRY_ADDR}:${REGISTRY_PORT}/${IMAGE_NAME}"
    echo "==> Building image ${image_ref}"
    docker_cmd build -t "${image_ref}" .
    echo "==> Pushing image ${image_ref}"
    docker_cmd push "${image_ref}"
}

up_registry() {
    docker_cmd compose up -d registry
}

up_cluster() {
    local size="$1"
    if [ "$size" -lt 1 ]; then
        echo "Error: size debe ser >= 1"
        exit 1
    fi

    docker_cmd compose up -d master

    local workers=$((size - 1))
    if [ "$workers" -gt 0 ]; then
        docker_cmd compose up -d --scale worker="$workers" worker
    else
        docker_cmd compose stop worker >/dev/null 2>&1 || true
        docker_cmd compose rm -f worker >/dev/null 2>&1 || true
    fi
}

down_cluster() {
    docker_cmd compose down
}

master_container_id() {
    docker_cmd compose ps -q master | head -n 1
}

exec_master() {
    if [ "$#" -eq 0 ]; then
        echo "Error: debes pasar un comando para exec"
        exit 1
    fi

    local cid
    cid=$(master_container_id)
    if [ -z "$cid" ]; then
        echo "Error: no encuentro el contenedor master en ejecucion"
        exit 1
    fi

    docker_cmd exec -u mpi "$cid" ash -lc "$*"
}

main() {
    local command=${1:-help}
    shift || true

    local size=4
    for arg in "$@"; do
        case "$arg" in
            size=*) size=${arg#size=} ;;
        esac
    done

    case "$command" in
        up)
            ensure_ssh_keys
            up_registry
            build_and_push
            up_cluster "$size"
            echo "Cluster listo: size=${size}, ssh_port=${SSH_PORT}"
            ;;
        reload)
            ensure_ssh_keys
            up_registry
            build_and_push
            down_cluster
            up_registry
            up_cluster "$size"
            echo "Cluster recargado: size=${size}, ssh_port=${SSH_PORT}"
            ;;
        scale)
            up_cluster "$size"
            echo "Cluster escalado: size=${size}, ssh_port=${SSH_PORT}"
            ;;
        down)
            down_cluster
            ;;
        list)
            docker_cmd compose ps
            ;;
        exec)
            exec_master "$@"
            ;;
        help|--help|-h)
            usage
            ;;
        *)
            usage
            exit 1
            ;;
    esac
}

main "$@"
