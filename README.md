# Sunda

**Sintaksis basajan. Arsitektur beresih. Runtime sahandapeun.**

Sunda mangrupa basa pamrograman umum anu di-implementasi kuning C++20 kalawan
lexer, parser, resolver, bytecode compiler, jeung stack-based virtual machine.
Filosofianna: gampang kawas Python, lentur kawas Lua, jeung tungtara
jeung kurung kurawal `{}` pikeun sakabéh blok.

## Nyitak mimiti

```sh
cmake -S . -B build
cmake --build build
./build/sunda run contoh.sd
```

Atawa tanpa CMake:

```sh
./devbuild.sh              # g++ langsung, hasilna build/sunda
./devbuild.sh sanitize     # + AddressSanitizer/UBSan
```

## CLI

```
sunda run <berkas.sd> [argumen]   ngajalankeun program
sunda repl                        sesi interaktif
sunda test [dir|berkas...]        ngajalankeun tés (standar: tests/)
sunda check <berkas.sd>           pariksa program tanpa ngajalankeun
sunda format <berkas.sd>          ngabéréskeun format kana stdout
```

Kode kaluar: `0` suksés, `64` usage, `65` galat kompilasi, `70` galat runtime,
`66` berkas henteu kapanggih, `1` aya tés gagal.

## Sintaksis

Kurung kurawal `{}` wajib pikeun sadaya blok. Indentasi ukur pikeun kameumeut,
moal ngarobah struktur program. Titik koma henteu wajib — baris anyar misahkeun
statement. `#` pikeun komentar. Tiping dinamis, henteu perlu deklarasi.

```sunda
ngaran = "Sunda"
umur = 18
aktif = bener

lamun umur >= 17 {
    cetak("Dewasa")
} lamunteu {
    cetak("Can dewasa")
}
```

### Keyword

`lamun` `lamunteu` `salila` `pikeun` `dina` `pagawean` `balikeun`
`eureun` `teruskeun` `kelas` `turunan` `ieu` `induk` `awal` `bener` `salah`
`kosong` `jeung` `atawa` `lain` `impor` `cobaan` `gagal`

### Pagawean

```sunda
pagawean tambah(a, b) {
    balikeun a + b
}

pagawean salam(ngaran, sapaan = "Halo") {   # nilai bawaan
    balikeun sapaan + ", " + ngaran
}

pagawean jumlahkeun(... angka) {            # parameter sésa (jadi daptar)
    total = 0
    pikeun a dina angka { total = total + a }
    balikeun total
}
```

Pagawean mangrupa nilai kelas satu: bisa disimpen, diteruskeun, sarta
di-pulangkeun. Closure ngerekam variabel ti lingkup sabudeureunana.

```sunda
pagawean buatCounter() {
    hitungan = 0
    pagawean naik() {
        hitungan = hitungan + 1
        balikeun hitungan
    }
    balikeun naik
}
c = buatCounter()
cetak(c(), c(), c())    # 1 2 3
```

**Aturan tugas:** tugas ka ngaran nu geus aya (lokal, upvalue, atawa global)
ngarobah nilai anu pangdeudeudeutna. Tugas ka ngaran nu can aya ngajieun
variabel — global di tingkat luhur, lokal di jero pagawean.

### Koleksi

```sunda
d = [3, 1, 2]          # daptar
k = {"a": 1, "b": 2}   # kamus
h = {1, 2, 3}          # himpunan
t = (1, "dua")         # tuple
r = rentang(0, 10, 2)  # rentang
```

Daptar: `tambah(x)` `cabut()` `leupaskeun(i)` `sisipkeun(i, x)` `indeks(x)`
`ngandung(x)` `urutkeun()` `balik()` `tiron()` `gabung(pemisah)`.
Kamus: `konci()` `nilai()` `ngandung(k)` `cabut(k)` `tiron()`.
Himpunan: `tambah(x)` `leupaskeun(x)` `ngandung(x)` `tiron()`.
Kecap: `gede()` `leutik()` `beberesit()` `ganti(lama, baru)` `indeks(s)`
`ngandung(s)` `meulah(pemisah)`.

Indeks negatif ngitung ti tukangeun (`d[-1]` nu terakhir). Slicing:
`d[1:3]`, `d[:2]`, `d[2:]`. Kamus henteu aya konci → `kosong`.

Iterasi ku `pikeun ... dina`: rentang, daptar, tuple, himpunan, kamus
(koncina), sarta kecap (per huruf).

### Kelas

```sunda
kelas Hewanan {
    pagawean awal(ngaran) {
        ieu.ngaran = ngaran
    }
    pagawean ngomong() {
        balikeun ieu.ngaran + "!"
    }
}

kelas Ucing turunan Hewanan {
    pagawean awal(ngaran) {
        induk.awal(ngaran)
    }
    pagawean ngomong() {
        balikeun induk.ngomong() + " meong"
    }
}
```

`awal` mangrupa konstruktor, `ieu` ngarujuk ka objek, `induk.metoda()`
ngahubungkeun ka implementasi inditna. Warisan tunggal, metoda bisa
di-override.

### Galat

```sunda
cobaan {
    gagal "ayeuna kaduruk"
} gagal (e) {
    cetak("ditangkap:", e)
}
```

`gagal` ngalungkeun galat (nilai kecap). Galat runtime — dibagi nol, variabel
henteu kapanggih, indeks di luar rentang, jst — ogé bisa ditangkap ku
`cobaan`; nilaina mangrupa pesen galatna. `gagal` di jero handler bakal
diteruskeun ka luar.

### Modul

```sunda
impor matematika        # modul standar
impor util              # berkas util.sd di sisi berkas nu ngimpor
cetak(matematika.akar(144))
cetak(util.dobel(21))
```

Modul berkas dieksekusi sakali, di-cache, sarta anggotana kabéh jadi
namespace-nya. Impor sirkular kedaduktukeun jeung jadi galat.

## Modul standar

- `matematika`: `pi` `e` `akar` `sin` `cos` `tan` `log` `lantai` `langit`
  `absolute` `bulat` `minimal` `maksimal`
- `random`: `acak()` `acak(a, b)` `pilih(d)` `siki(n)`
- `waktu`: `ayeuna()` `nitih()` `sare(d)`
- `file`: `baca(path)` `tulis(path, isi)` `tambahan(path, isi)` `aya(path)`
  `hapus(path)`
- `sistem`: `argumen()` `direktori()`

Pagawean bawaan: `cetak(...)` `tulis(...)` `panjang(x)` `jenis(x)` `kecap(x)`
`angka(x)` `rentang(...)` `himpun(x)` `asupan()`.

## Arsitektur

```
src/
  token.h       definisi token + utilitas galat
  lexer.{h,cpp} scanning, penanganan baris anyar, mode format
  parser.{h,cpp} Pratt parser, AST
  ast.h         node AST
  resolver.{h,cpp} lingkup, slot lokal, upvalue, konteks kelas
  compiler.{h,cpp}  bytecode per pagawean, constant pool
  chunk.{h,cpp} opcode + bytecode
  value.h       NaN-boxing: angka inline, objek ku pointer, nil/bool bertag
  object.{h,cpp}    objek runtime (string, closure, kelas, koleksi, ...)
  table.{h,cpp} hash table (open addressing + tombstone)
  memory.{h,cpp}    heap + mark-and-sweep GC
  vm.{h,cpp}    virtual machine, call frames, iterasi, galat
  native.{h,cpp}    pagawean bawaan + metoda koleksi
  modules.{h,cpp}   modul standar
  formatter.{h,cpp} sunda format
  cli.{h,cpp}   run/repl/test/check/format
  main.cpp      titik asupan
```

Alur eksekusi: source → `Lexer` → token → `Parser` → AST → `Resolver`
(ngaitung slot lokal, upvalue, sarta ngadeklarasikeun variabel) → `Compiler`
→ bytecode per pagawean → `VM` ngajalankeun ku call frames. GC mark-and-sweep
ngalumbarkeun tina titik alokasi; akar: tumpukan, frame, global modul,
upvalue terbuka, cache modul, sarta table metoda. `SUNDA_GC_STRESS=1`
ngajalankeun GC di unggal alokasi pikeun tés.

Uji: `tests/*.sd` jeung `*.expected` + `*.exit` (kode kaluar). Jalankeun ku
`sh tests/run_tests.sh build/sunda` atawa `sunda test`.
Benchmark: `bench/run_bench.sh build/sunda`.

## Batasan

- Tidak ada konversi angka ↔ kecap otomatis (pakai `angka()`/`kecap()`).
- koleksi dibandingkeun ku identitas, henteu ku isi.
- Closure teu bisa di-serialize; bytecode henteu bisa disimpen ka berkas (plan
  `sunda build`).
- Unicode diperlakakeun salaku UTF-8 pikeun iterasi kecap jeung `panjang`,
  samentara `gede()`/`leutik()` ngan ASCII.
