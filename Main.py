import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import subprocess
import os
import re
import sys
import ctypes
import threading
import time

# ------------------------------------------------------------
# 1. Checking the Administrator
# ------------------------------------------------------------
if not ctypes.windll.shell32.IsUserAnAdmin():
    ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, " ".join(sys.argv), None, 1)
    sys.exit()

# ------------------------------------------------------------
# 2. Translates
# ------------------------------------------------------------
LANG = {
    "en": {
        "title": "USB Creator",
        "select_iso": "1. Select ISO file:",
        "select_usb": "2. Select USB drive:",
        "browse": "Browse",
        "refresh": "Refresh USB",
        "start": "START",
        "working": "Working...",
        "ready": "Ready",
        "done": "Done!",
        "warning": "WARNING: This will FORMAT your USB drive. All data will be lost. Continue?",
        "error": "Error",
        "no_iso": "Please select an ISO file.",
        "no_usb": "Please select a USB drive.",
        "lang": "Language:",
        "step1": "1. Formatting USB...",
        "step2": "2. Mounting ISO...",
        "step3": "3. Copying files to USB...",
        "step4": "4. Dismounting ISO...",
        "step5": "5. Verifying file system...",
        "step6": "6. Done! Bootable USB created."
    },
    "ru": {
        "title": "USB Creator",
        "select_iso": "1. Выберите ISO-файл:",
        "select_usb": "2. Выберите USB-накопитель:",
        "browse": "Обзор",
        "refresh": "Обновить USB",
        "start": "СТАРТ",
        "working": "Выполняется...",
        "ready": "Готов",
        "done": "Готово!",
        "warning": "ВНИМАНИЕ: Это ОТФОРМАТИРУЕТ ваш USB. Все данные будут удалены. Продолжить?",
        "error": "Ошибка",
        "no_iso": "Выберите ISO-файл.",
        "no_usb": "Выберите USB-накопитель.",
        "lang": "Язык:",
        "step1": "1. Форматирование USB...",
        "step2": "2. Монтирование ISO...",
        "step3": "3. Копирование файлов на USB...",
        "step4": "4. Извлечение ISO...",
        "step5": "5. Проверка файловой системы...",
        "step6": "6. Готово! Загрузочный USB создан."
    }
}

# ------------------------------------------------------------
# 3. Getting lists of drivers
# ------------------------------------------------------------
def get_usb_drives():
    try:
        result = subprocess.run(['wmic', 'logicaldisk', 'where', 'DriveType=2', 'get', 'DeviceID'],
                                capture_output=True, text=True, check=True,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        drives = re.findall(r'([A-Z]:)', result.stdout)
        return drives
    except:
        return ["E:", "F:", "G:"]

# ------------------------------------------------------------
# 4. Formating to Fat32
# ------------------------------------------------------------
def format_usb(drive_letter):
    diskpart_script = f"""
select volume {drive_letter[:-1]}
clean
create partition primary
format fs=fat32 quick
assign letter={drive_letter[:-1]}
active
exit
"""
    script_path = os.path.join(os.environ['TEMP'], 'diskpart_script.txt')
    with open(script_path, 'w') as f:
        f.write(diskpart_script)
    
    result = subprocess.run(['diskpart', '/s', script_path],
                            capture_output=True, text=True,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    os.remove(script_path)
    
    if result.returncode != 0:
        raise Exception("Не удалось отформатировать диск для загрузки.")
    
    return True

# ------------------------------------------------------------
# 5. Writing the ISO (maybe the scariest part)
# ------------------------------------------------------------
def write_iso_to_usb(iso_path, drive_letter, progress_callback=None):
    try:
        # Step 1: Formating
        progress_callback(10, LANG["ru"]["step1"])
        format_usb(drive_letter)
        
        # Step 2: ISO
        progress_callback(25, LANG["ru"]["step2"])
        mount_cmd = [
            'powershell', '-ExecutionPolicy', 'Bypass',
            '-Command', f'$mount = Mount-DiskImage -ImagePath "{iso_path}" -PassThru; $mount'
        ]
        subprocess.run(mount_cmd, capture_output=True, text=True,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        
        # Getting the letter ISO
        get_drive_cmd = [
            'powershell', '-ExecutionPolicy', 'Bypass',
            '-Command', f'(Get-DiskImage -ImagePath "{iso_path}" | Get-Volume).DriveLetter'
        ]
        result = subprocess.run(get_drive_cmd, capture_output=True, text=True,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        iso_drive_letter = result.stdout.strip()
        
        if not iso_drive_letter:
            raise Exception("Не удалось определить букву смонтированного ISO.")
        
        # ШАГ 3: Copying the files with robocopy
        progress_callback(40, LANG["ru"]["step3"])
        robocopy_cmd = [
            'robocopy', f'{iso_drive_letter}:\\', f'{drive_letter}\\',
            '/E', '/R:0', '/W:0', '/NJH', '/NJS', '/NP'
        ]
        process = subprocess.Popen(robocopy_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        
        for line in iter(process.stdout.readline, ''):
            if '%' in line:
                try:
                    percent = int(line.strip().split('%')[0])
                    progress_callback(40 + int(percent * 0.4), LANG["ru"]["step3"])
                except:
                    pass
        
        process.wait()
        
        # Step 3: Extracting the ISO
        progress_callback(85, LANG["ru"]["step4"])
        dismount_cmd = [
            'powershell', '-ExecutionPolicy', 'Bypass',
            '-Command', f'Dismount-DiskImage -ImagePath "{iso_path}"'
        ]
        subprocess.run(dismount_cmd, capture_output=True, text=True,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        
        # Step 4: Checking the File System (Boot folder
        progress_callback(95, LANG["ru"]["step5"])
        boot_folder = f"{drive_letter}\\boot"
        if not os.path.exists(boot_folder):
            time.sleep(2)
            if not os.path.exists(boot_folder):
                raise Exception("На флешке не появились загрузочные файлы.")
        
        # ШАГ 6: Done!
        progress_callback(100, LANG["ru"]["step6"])
        
        return True
    except Exception as e:
        raise e

# ------------------------------------------------------------
# 6. UI
# ------------------------------------------------------------
class USBCreatorApp:
    def __init__(self, root):
        self.root = root
        self.current_lang = "ru"
        self.root.title(LANG[self.current_lang]["title"])
        self.root.geometry("550x450")
        self.root.resizable(False, False)
        self.build_ui()

    def build_ui(self):
        for widget in self.root.winfo_children():
            widget.destroy()

        # Language
        lang_frame = tk.Frame(self.root)
        lang_frame.pack(pady=5, anchor="w", padx=20)
        tk.Label(lang_frame, text=LANG[self.current_lang]["lang"]).pack(side="left", padx=5)
        lang_combo = ttk.Combobox(lang_frame, values=["en", "ru"], state="readonly", width=5)
        lang_combo.set(self.current_lang)
        lang_combo.pack(side="left")
        lang_combo.bind("<<ComboboxSelected>>", lambda e: self.change_lang(lang_combo.get()))

        # ISO
        tk.Label(self.root, text=LANG[self.current_lang]["select_iso"]).pack(anchor="w", padx=20, pady=(15, 5))
        self.iso_path_var = tk.StringVar()
        iso_frame = tk.Frame(self.root)
        iso_frame.pack(fill="x", padx=20)
        tk.Entry(iso_frame, textvariable=self.iso_path_var, width=40).pack(side="left")
        tk.Button(iso_frame, text=LANG[self.current_lang]["browse"], command=self.browse_iso).pack(side="left", padx=5)

        # USB
        tk.Label(self.root, text=LANG[self.current_lang]["select_usb"]).pack(anchor="w", padx=20, pady=(15, 5))
        self.usb_var = tk.StringVar()
        self.usb_combo = ttk.Combobox(self.root, textvariable=self.usb_var, values=get_usb_drives(), state="readonly", width=20)
        self.usb_combo.pack(anchor="w", padx=20)
        tk.Button(self.root, text=LANG[self.current_lang]["refresh"], command=self.refresh_usb).pack(anchor="w", padx=20, pady=5)

        # Start button
        self.btn_start = tk.Button(
            self.root,
            text=LANG[self.current_lang]["start"],
            command=self.start_write,
            bg="#4CAF50", fg="white", font=("Arial", 12, "bold")
        )
        self.btn_start.pack(pady=30)

        # Progress
        self.progress = ttk.Progressbar(self.root, orient="horizontal", length=400, mode="determinate")
        self.progress.pack(pady=10)

        # Status
        self.lbl_status = tk.Label(self.root, text=LANG[self.current_lang]["ready"], fg="gray")
        self.lbl_status.pack()

    def change_lang(self, code):
        self.current_lang = code
        self.build_ui()

    def browse_iso(self):
        file_path = filedialog.askopenfilename(
            title="Select Windows ISO",
            filetypes=[("ISO files", "*.iso"), ("All files", "*.*")]
        )
        if file_path:
            self.iso_path_var.set(file_path)

    def refresh_usb(self):
        self.usb_combo['values'] = get_usb_drives()

    def update_progress(self, percent, message):
        self.progress['value'] = percent
        self.lbl_status.config(text=message)

    def start_write(self):
        iso = self.iso_path_var.get()
        usb = self.usb_var.get()

        if not iso:
            messagebox.showerror(LANG[self.current_lang]["error"], LANG[self.current_lang]["no_iso"])
            return
        if not usb:
            messagebox.showerror(LANG[self.current_lang]["error"], LANG[self.current_lang]["no_usb"])
            return

        if not messagebox.askyesno("Warning", LANG[self.current_lang]["warning"]):
            return

        self.btn_start.config(state="disabled", text=LANG[self.current_lang]["working"])
        self.progress['value'] = 0
        self.lbl_status.config(text=LANG[self.current_lang]["step1"])

        def work():
            try:
                def prog(p, msg):
                    self.root.after(0, lambda: self.update_progress(p, msg))
                success = write_iso_to_usb(iso, usb, prog)
                if success:
                    self.root.after(0, lambda: self.lbl_status.config(text=LANG[self.current_lang]["done"], fg="green"))
                    self.root.after(0, lambda: messagebox.showinfo("Done", LANG[self.current_lang]["done"]))
            except Exception as e:
                error_text = str(e)
                self.root.after(0, lambda: self.lbl_status.config(text="Error", fg="red"))
                self.root.after(0, lambda: messagebox.showerror(LANG[self.current_lang]["error"], error_text))
            finally:
                self.root.after(0, lambda: self.btn_start.config(state="normal", text=LANG[self.current_lang]["start"]))

        threading.Thread(target=work, daemon=True).start()

# ------------------------------------------------------------
# 7. Run
# ------------------------------------------------------------
if __name__ == "__main__":
    root = tk.Tk()
    app = USBCreatorApp(root)
    root.mainloop()