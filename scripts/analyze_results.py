#!/usr/bin/env python3
import argparse
import csv
import html
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev


def scenario_label(row):
    nodes = row["nodes"]
    np = row["np"]
    ppn = row["ppn"]
    if nodes == "1":
        return "1 nodo"
    if nodes == "4" and ppn == "2":
        return "4 nodos, 2 ppn"
    return f"{nodes} nodos"


def read_rows(path):
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = []
        for row in reader:
            if "variant" not in row or row["variant"] in (None, ""):
                row["variant"] = "mpi_matrix"
            rows.append(row)
        return rows


def aggregate(rows):
    grouped = defaultdict(list)
    for row in rows:
        key = (
            row["variant"],
            int(row["size"]),
            int(row["nodes"]),
            int(row["np"]),
            int(row["ppn"]),
        )
        grouped[key].append((float(row["seconds"]), float(row["gflops"]), row["checksum"]))

    summary = []
    for key, values in grouped.items():
        seconds = [v[0] for v in values]
        gflops = [v[1] for v in values]
        checksums = {v[2] for v in values}
        variant, size, nodes, np, ppn = key
        summary.append(
            {
                "variant": variant,
                "size": size,
                "nodes": nodes,
                "np": np,
                "ppn": ppn,
                "scenario": scenario_label(
                    {"nodes": str(nodes), "np": str(np), "ppn": str(ppn)}
                ),
                "runs": len(values),
                "seconds_mean": mean(seconds),
                "seconds_stdev": stdev(seconds) if len(seconds) > 1 else 0.0,
                "gflops_mean": mean(gflops),
                "gflops_stdev": stdev(gflops) if len(gflops) > 1 else 0.0,
                "checksum_ok": len(checksums) == 1,
            }
        )
    summary.sort(key=lambda r: (r["variant"], r["size"], r["nodes"], r["ppn"], r["np"]))
    return summary


def add_speedups(summary):
    baseline = {}
    for row in summary:
        if row["nodes"] == 1 and row["np"] == 1 and row["ppn"] == 1:
            baseline[(row["variant"], row["size"])] = row["seconds_mean"]

    for row in summary:
        base = baseline.get((row["variant"], row["size"]))
        row["speedup"] = base / row["seconds_mean"] if base else None
        row["efficiency"] = row["speedup"] / row["np"] if row["speedup"] else None


def write_markdown(summary, output_path, csv_path, chart_paths):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Resumen de resultados MPICH",
        "",
        f"CSV analizado: `{csv_path}`.",
        "",
        "La tabla usa la media de las repeticiones. El speedup se calcula frente a la ejecucion con 1 nodo, 1 proceso y el mismo tamano de matriz.",
        "",
        "| Variante | Tamano | Escenario | Reps | Tiempo medio (s) | GFLOPS | Speedup | Eficiencia | Checksum |",
        "|---|---:|---|---:|---:|---:|---:|---:|---|",
    ]

    for row in summary:
        speedup = "-" if row["speedup"] is None else f"{row['speedup']:.2f}"
        efficiency = "-" if row["efficiency"] is None else f"{row['efficiency']:.2f}"
        checksum = "ok" if row["checksum_ok"] else "revisar"
        lines.append(
            "| {variant} | {size} | {scenario} | {runs} | {seconds:.6f} | {gflops:.3f} | {speedup} | {efficiency} | {checksum} |".format(
                variant=row["variant"],
                size=row["size"],
                scenario=row["scenario"],
                runs=row["runs"],
                seconds=row["seconds_mean"],
                gflops=row["gflops_mean"],
                speedup=speedup,
                efficiency=efficiency,
                checksum=checksum,
            )
        )

    if chart_paths:
        lines.extend(["", "## Graficas", ""])
        for chart in chart_paths:
            rel = chart.relative_to(output_path.parent)
            lines.append(f"- [{chart.stem}]({rel.as_posix()})")

    lines.extend(
        [
            "",
            "## Lectura rapida",
            "",
            "- Si el speedup crece menos que el numero de procesos, el coste de comunicacion y sincronizacion esta limitando la mejora.",
            "- Si 4 nodos con 2 procesos por nodo no mejora frente a 4 nodos con 1 proceso por nodo, la maquina anfitriona ya esta saturando CPU, memoria o red virtual.",
            "- Si el checksum cambia entre escenarios para la misma variante y tamano, hay que revisar reparto, recogida o inicializacion.",
        ]
    )
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_svg_chart(rows, path):
    width = 840
    height = 420
    margin_left = 70
    margin_bottom = 80
    margin_top = 36
    margin_right = 28
    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom
    max_value = max(row["gflops_mean"] for row in rows) or 1.0
    bar_gap = 18
    bar_w = max(28, (plot_w - bar_gap * (len(rows) + 1)) / max(1, len(rows)))

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        f'<text x="{margin_left}" y="24" font-family="Arial" font-size="18" fill="#111827">{html.escape(path.stem)}</text>',
        f'<line x1="{margin_left}" y1="{height - margin_bottom}" x2="{width - margin_right}" y2="{height - margin_bottom}" stroke="#111827"/>',
        f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{height - margin_bottom}" stroke="#111827"/>',
    ]

    for i in range(5):
        value = max_value * i / 4
        y = height - margin_bottom - (value / max_value) * plot_h
        parts.append(f'<line x1="{margin_left}" y1="{y:.1f}" x2="{width - margin_right}" y2="{y:.1f}" stroke="#e5e7eb"/>')
        parts.append(f'<text x="12" y="{y + 4:.1f}" font-family="Arial" font-size="12" fill="#4b5563">{value:.1f}</text>')

    palette = ["#2563eb", "#059669", "#d97706", "#7c3aed"]
    for idx, row in enumerate(rows):
        x = margin_left + bar_gap + idx * (bar_w + bar_gap)
        bar_h = (row["gflops_mean"] / max_value) * plot_h
        y = height - margin_bottom - bar_h
        color = palette[idx % len(palette)]
        label = html.escape(row["scenario"])
        parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_w:.1f}" height="{bar_h:.1f}" fill="{color}"/>')
        parts.append(f'<text x="{x + bar_w / 2:.1f}" y="{y - 6:.1f}" text-anchor="middle" font-family="Arial" font-size="12" fill="#111827">{row["gflops_mean"]:.1f}</text>')
        parts.append(f'<text x="{x + bar_w / 2:.1f}" y="{height - margin_bottom + 18}" text-anchor="middle" font-family="Arial" font-size="11" fill="#374151">{label}</text>')

    parts.append(f'<text x="{margin_left}" y="{height - 18}" font-family="Arial" font-size="12" fill="#4b5563">GFLOPS medios por escenario</text>')
    parts.append("</svg>")
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def write_charts(summary, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    chart_paths = []
    by_variant_size = defaultdict(list)
    for row in summary:
        by_variant_size[(row["variant"], row["size"])].append(row)

    for (variant, size), rows in sorted(by_variant_size.items()):
        rows.sort(key=lambda r: (r["nodes"], r["ppn"], r["np"]))
        path = output_dir / f"gflops_{variant}_{size}.svg"
        write_svg_chart(rows, path)
        chart_paths.append(path)
    return chart_paths


def main():
    parser = argparse.ArgumentParser(description="Resume resultados CSV de la practica MPICH.")
    parser.add_argument("csv", nargs="?", default="results.csv")
    parser.add_argument("--out", default="docs/results_summary.md")
    parser.add_argument("--charts-dir", default="docs/charts")
    args = parser.parse_args()

    csv_path = Path(args.csv)
    summary = aggregate(read_rows(csv_path))
    add_speedups(summary)
    chart_paths = write_charts(summary, Path(args.charts_dir))
    write_markdown(summary, Path(args.out), csv_path, chart_paths)
    print(f"Resumen escrito en {args.out}")
    print(f"Graficas escritas en {args.charts_dir}")


if __name__ == "__main__":
    main()
