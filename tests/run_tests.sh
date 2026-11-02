binary="${1:-build/sunda}"
directory="$(cd "$(dirname "$0")" && pwd)"
passed=0
failed=0
for source in "$directory"/*.sd; do
    expected="${source%.sd}.expected"
    [ -f "$expected" ] || continue
    actual="$("$binary" run "$source" 2>/dev/null)"
    status=$?
    expectedStatus=0
    if [ -f "${source%.sd}.exit" ]; then
        expectedStatus="$(cat "${source%.sd}.exit")"
    fi
    if [ "$actual" = "$(cat "$expected")" ] && [ "$status" -eq "$expectedStatus" ]; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
        echo "GAGAL: $(basename "$source") (kode kaluar: $status, diarepkeun: $expectedStatus)"
    fi
done
echo "lulus $passed, gagal $failed"
[ "$failed" -eq 0 ]
