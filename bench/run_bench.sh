binary="${1:-build/sunda}"
directory="$(cd "$(dirname "$0")" && pwd)"
for bench in "$directory"/*.sd; do
    name="$(basename "$bench" .sd)"
    start="$(date +%s%N)"
    result="$("$binary" run "$bench" 2>/dev/null)"
    status=$?
    end="$(date +%s%N)"
    ms=$(( (end - start) / 1000000 ))
    printf "%-10s %6d ms  (hasil=%s, kode=%d)\n" "$name" "$ms" "$result" "$status"
done
