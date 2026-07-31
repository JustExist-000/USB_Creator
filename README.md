# USB Creator

A lightweight Windows utility to create bootable USB drives from ISO files.  
Built in Python as a personal learning project (in one day).

---

## What it does
- Formats a USB drive to FAT32.
- Mounts a Windows ISO file.
- Copies all files from the ISO to the USB drive.
- Creates a bootable Windows USB.

## How to use
1. Run `USB_Creator.exe` as Administrator.
2. Select your Windows ISO file.
3. Select your USB drive.
4. Press the START button and wait.

---

## Important note
This is an **educational project** made in one day.  
It works, but for daily use or critical tasks, please use the original **[Rufus](https://rufus.ie/)**.

## Built with
- Python + Tkinter (GUI) / C++ + MinGW64
- Windows built-in tools: `diskpart`, `robocopy`, PowerShell

---

**Author:** [JustExist-000](https://github.com/JustExist-000)  
**Date:** 30 July 2026
