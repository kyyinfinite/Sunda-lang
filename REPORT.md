# Laporan Téknis — Evolusi Basa Sunda

Tanggalkapan: 2026-11-02 · Basis: répositori Sunda pamekaran C++20 (≈2.400 baris)

## 1. Audit arsitektur awal

Kondisi répositori asal (verifikasi: build + 12 tés lulus, 12/12):

| Komponén  | Kaayaan                                                                 |
|-----------|-------------------------------------------------------------------------|
| Lexer     | Émut heuristik "newline sanggeus token tungtung-statement"; newline di jero `()`/`[]` ngarusak ekspresi multibaris |
| Parser    | `citak` statement husus; token `pagawean`, `balikeun`, `kelas`, `impor`, `cobaan`, `gagal` didefinisikan tapi **teu pernah di-parse** |
| AST       | 9 jenis ekspresi, 9 statement; teu aya koleksi, fungsi, kelas           |
| Resolver  | Slot lokal datar; teu aya lingkup pagawean                              |
| Compiler  | Hiji chunk pikeun sakabéh program; teu aya panggilan fungsi             |
| VM        | Computed goto, tapi `Call` ngan bisa ngahubung native; teu aya call frame |
| Objek     | Ngan `String` + `Native`; `valueToString` boga fallback `<objek>`       |
| Heap      | Ngaitung `bytesAllocated` tapi **GC moal pernah lumangsung**; byte string henteu diitung |
| Native    | 4 fungsi (`jenis` `kecap` `angka` `panjang`); `citak` mangrupa opcode, henteu fungsi |
| CLI       | Ngan `sunda <berkas>`; teu aya repl/test/check/format                   |
| Tés       | `run_tests.sh` ngabandingkeun stdout wungkul, **abaikan kode kaluar**    |

Probe tambahan parantos nga-mastikeun: `[1, 2]` jeung `{"a": 1}` = galat
kompilasi; `cetak("a",\n "b")` gagal; rékursi henteu mungkin; GC henteu aya.

## 2. Masalah anu diidentifikasi

1. **Frontend teu lengkep** — setengah keyword tanpa grammar; statement `citak`
   robustus bakal ngahalangan `cetak()` salaku fungsi normal.
2. **Teu aya model panggilan** — teu aya frame, teu aya rékursi, teu aya closure.
3. **Teu aya koleksi** — daptar/kamus/himpunan/tuple/rentang sadayana absén.
4. **Kebocoran memori struktural** — lokal blok ditinggalkeun dina tumpukan,
   `eureun` dina pikeun ngabocorkeun slot, GC henteu pernah ngumpul.
5. **Kasalahan pangujian** — runner henteu mariksa kode kaluar; tés galat
   lulus kumaha ogé.
6. **Heuristik baris anyar** — newline di jero kurung ngarusak ekspresi
   multibaris (pangrubungan explorer vs. blok `{}`).
7. **Duplikasi tanggung jawab** — `citak` salaku statement + opcode + `printValues`
   dina VM, sedengkeun native table awéwé.

## 3. Kaputusan arsitektur

- **Kurung kurawal tetep resmi**; indentasi moal jadi mekanisme parsing.
- **`cetak` jadi native variadic** — opcode `Print`, statement `PrintStmt`,
  jeung keyword `citak` dihapus; hiji mekanisme output (fwrite) pikeun sadayana.
- **`pikeun` ngagantikeun `unggal`** pikeun iterasi (contoh resmi spesifikasi),
  ngahindarkeun dua keyword pikeun operasi nu sarua. `salila` tetep.
- **Model nilai**: NaN-boxing 48-bit ditahan (parobihan minor).
- **Slot vs. value-left**: tingkat luhur ngagunakeun model "nilai ditinggalkeun
  dina tumpukan" (kasaluyuan sareng paripolah asal), fungsi ngagunakeun slot
  prealokasi (`stackTop = slots + slotCount`) — ngahindarkeun clobber lokal,
  nyababkeun lokal obejek mimiti `kosong`.
- **Semantika tugas**: ngarebind penikatan pangdeudeudeutna (lokal → upvalue →
  global); ngaran anyar = global di tingkat luhur, lokal di fungsi. Pra-scan
  tingkat luhur mastikeun global terkenal sateuacan bodi pagawean di-resolve.
- **Upvalue ala-clox** kalawan buka/tutup, dilerem per alamat, sarta
  `CloseUpvalueN`/`CloseUpvalueFrom` pikeun loop/blok. Descriptor didata
  kapamilikanana: fungsi nu ngarebut nambahkeun desc ka dirina sorangan,
  ngarujuk lokal indungna langsung.
- **GC mark-and-sweep**: akar = tumpukan (base→stackTop), frame closure +
  global modul, cache modul, table metoda, upvalue terbuka, tempRoots pikeun
  native; interned string paé diberesihan tina intern table saméméh sweep.
  Jandela GC dijaga (pause/resume) sabudeureun kompilasi, loading modul,
  sarta registrasi bawaan; slot frame di-nil-keun mangsa pamanggilan.
- **Galat = nilai**: sadaya galat runtime jadi nilai kecap nu tiasa ditangkup;
  catch point jadi **tumpukan per frame** (nested cobaan di frame nu sarua).
- **Modul** = `ObjModule` nu boga table global sorangan; builtin table global
  dipaké babarengan (fallback `GetGlobal`); eksekusi modul = frame anyar
  dina loop VM nu sarua.
- **CLI**: paréntah disebutkeun dina spesifikasi dilaksanakeun pisan:
  `run` `repl` `test` `check` `format`. `build` **teu** dilaksanakeun (néangan
  serialisasi bytecode — dicatet salaku fase saterusna) sangkan henteu aya
  paréntah palsu.

## 4. Berkas anu dirobih

`src/token.h`, `src/lexer.{h,cpp}`, `src/ast.h`, `src/parser.{h,cpp}`,
`src/resolver.{h,cpp}`, `src/compiler.{h,cpp}`, `src/chunk.h`,
`src/value.h` (minor), `src/object.{h,cpp}`, `src/table.{h,cpp}`,
`src/memory.{h,cpp}`, `src/vm.{h,cpp}`, `src/native.{h,cpp}`,
`src/main.cpp`, `CMakeLists.txt`, `tests/run_tests.sh`, sadaya tés lama
(`citak`→`cetak`, `unggal`→`pikeun`).

## 5. Berkas anu dijieun

`src/modules.{h,cpp}` (modul standar), `src/formatter.{h,cpp}`,
`src/cli.{h,cpp}`, `tools/disasm.cpp` (disassembler debug), `devbuild.sh`
(build g++ pikeun lingkungan tanpa cmake), 21 berkas tés anyar + expected/exit,
5 benchmark + `bench/run_bench.sh`, `README.md`, ieu laporan.

## 6. Fitur anu dilaksanakeun (sadaya terverifikasi ku tés)

- **Fungsi**: deklarasi `pagawean`, `balikeun` (nilai opsional), rékursi
  (bateas 1024 frame + kapasitas tumpukan), fungsi nested, closure kalawan
  rebutan per-iterasi di loop, pagawean kelas-satu, argumen bawaan
  (dieusi `kosong` → ekspresi bawaan), variadic `...sésa`, pamriksaan jumlah
  argumen kalawan pesen jelas.
- **Koleksi**: daptar, kamus (literal `{k: v}` dibedakan ku konteks parser),
  himpunan, tuple, rentang; indexing (indeks negatif), slicing, 30+ metoda,
  `panjang` pikeun sadaya; iterasi generik (String UTF-8, Rentang, Daptar,
  Tuple, Kamus konci, Himpunan).
- **Kelas**: `kelas`, warisan tunggal ku `turunan`, `awal` konstruktor
  (otomatis mulangkeun instance), `ieu`, `induk.metoda()`, override,
  bound method salaku nilai, atribut via `ieu.x`.
- **Galat**: `cobaan/gagal(e)`, nested, naliwat frame, rethrow; galat runtime
  kabéh tangtab; kode kaluar 65/70 bener.
- **Modul**: `impor nama` (native + berkas, relatif ka diréktori nu ngimpor),
  cache, deteksi sirkular, namespace; modul standar matematika/random/waktu/
  file/sistem.
- **Memori**: mark-and-sweep GC kalawan akar lengkep; tés memori kaluaran
  300×(string+kamus+himpunan+tuple) + 50 closure + 100 objek lulus di
  `SUNDA_GC_STRESS=1` (GC unggal alokasi) kalawan ASan/UBSan bersih.
- **Kaperluan pamekar**: `sunda run/repl/test/check/format` (sadaya nyata),
  REPL multiline kalawan echo hasil, `sunda test` ngamarikeun stdout **jeung**
  kode kaluar (saprak ini di-fitur), formatter idempoten (uji:
  format(format(x)) == format(x), sarta output program terawat).

## 7. Hasil build

- `g++ 14.2 -std=c++20 -Wall -Wextra -Wpedantic -O2`: **bersih, 0 warning,
  0 error** (sanggeus ngabéréskeun sésa peringatan).
- Build ASan/UBSan: bersih; tés lengkep + GC stress: 0 temuan.
- CMake target dipigaronakeun (`sunda` lib + `sunda_cli` + CTest `sunda_tests`,
  `sunda_cli_tests`); `devbuild.sh` pikeun lingkungan tanpa cmake.
- `tools/disasm.cpp` disadiakeun pikeun verifikasi bytecode manual.

## 8. Hasil tés

`sh tests/run_tests.sh build/sunda` → **lulus 33, gagal 0** (sadaya ogé
ngamarikeun kode kaluar; `galat_*` nyekel 65/70 sasuai). Cakupan: aritmatika,
logika, kecap, kondisi, loop (salila/pikeun), multibaris, lingkup, konversi,
variabel, galat runtime/sintaks/lingkup/jenis/indeks/panggilan/argumen/
impor/rékursi, fungsi, closure, daftar, kamus, himpunan+tuple, rentang+iterasi,
kelas, cobaan/gagal, impor std/berkas, memori, sintaks.

## 9. Hasil benchmark

Intel i7-kelas sandbox, g++ -O2, median ti 3 larian:

| Benchmark | Pagerak                                             | Waktu  |
|-----------|-----------------------------------------------------|--------|
| fib       | fib(27) rékursif (≈636k panggilan)                  | 25 ms  |
| loop      | salila 1M iterasi, akumulasi                        | 50 ms  |
| objek     | 20k instansiasi kelas + metoda                      | 8 ms   |
| daftar    | 20k tambah + urutkeun + iterasi                     | 4 ms   |
| string    | 5k konkatenasi (O(n²))                              | 18 ms  |

Konstanta leungitkeun (`Constant` folding pikeun literal angka) lumangsung
dina kompilasi; dispatch computed-goto ditahan ti kode asal.

## 10. Katerbatasan anu sésa

- Bytecode henteu tasikeun/ka-berkas → `sunda build` sengaja teu aya.
- Henteu aya lambdas anonim (pagawean nested ngalicikkeun kasus ieu).
- Perbandingan koleksi ku identitas; `==` kamus/daptar henteu struktural.
- `gede()`/`leutik()` ASCII wungkul; internasionalisasi huruf teu lengkep.
- REPL nyimpen anggota global antar baris, tapi definisi blok teu lintas
  baris iwal di jero `{}` nu dibuka (didukung ku pelacak kurung).
- Galat native henteu ngabawa traceback multi-frame (ngan frame asal).
- Kamus kalawan konci objek: hashing ku identitas pointer — sah, tapi henteu
  stabif antar larian.

## 11. Fase pamekar anu disarankeun saterusna

1. **Serialisasi bytecode** → `sunda build` + loader (dasar: format chunk
   parantos stabil; objFunction teu boga pointer luar iwal string interned).
2. **Interpolasi & API kecap** (pad, format), metoda daptar `map`/`saring`.
3. **LSP + debugger hooks**: frame + line table parantos cekap pikeun
   breakpoints; `tools/disasm.cpp` salaku fondasi tracer.
4. **Optimasi terukur**: inline cache pikeun `GetProperty`/`Invoke`,
   superinstruction `GetLocal+Constant`, konstanta global caturwulan.
5. **Modul ecosistem**: `impor` kalawan jalur paket + manajer saderhana
   (sanggeus loader stabil, sapertos anu disebutkeun dina spesifikasi).

---

**Kriteria panarimaan**: kabéh tiék dina spésifikasi §19 parantos disangkalíkan
— build suksés, program lama nu sah tetep jalan (12 tés asal dimigrasi, teu
ayeuna), tés anyar pinuh, kode kaluar bener, `{}` tetep resmi, `cetak()`
via native, tanggung jawab modul jelas, euweuh implementasi palsu,
dokumentasi cocog kalawan paripolah.
