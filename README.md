# PB CHEAT — PEYXDEV

Fitur: **Crosshair titik** + **NameTag ESP** + **Fast Reload**.

## Pakai
1. Klik `JALANKAN PB CHEAT` di Desktop → UAC → Yes.
2. **INSERT** = buka/minimize menu, **X** = tutup menu, **END** = unload.
3. **F6** = Fast Reload (makro `R → 3 → 1`, bisa diatur di config).

## Fitur
- **Crosshair** — sekarang **titik** (bukan lingkaran). Ukuran `8/12/16/20` di config, warna `crosshair_r/g/b`. Overlay hanya di-invalidate saat pengaturan berubah → tidak kedip.
- **NameTag ESP** — patch AOB `C6 01 00 8D 49 08 83 E8 01 75 F5` di `PointBlank.exe+0x79EB1`; nama musuh tembus dinding.
- **Fast Reload** — makro native tombol `R → 3 → 1` untuk memotong animasi reload. Hotkey F6, delay bisa diatur (`reload_hold_ms`, `reload_gap_ms`).

## Tidak tersedia
**Movement speed** dan **auto headshot** tidak saya pasang: keduanya butuh alamat memori (offset speed / bone matrix / punch angle) yang belum bisa diverifikasi. Saya cek 3 repo PB publik — tidak ada patch AOB untuk itu. Mengarang offset hanya akan bikin game crash atau fitur diam.

## Config
`config/offsets.cfg` — auto-reload saat file berubah.

## Build
`build.bat` → `pb-cheat.dll`, `injector.exe`, `launcher.exe`. Toolchain: `C:\Users\Peyx\tools\mingw32`.
