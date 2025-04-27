import tkinter as tk
from tkinter import ttk, messagebox
import socket
import threading

SOCKET_PATH = "/tmp/manager_socket"
REFRESH_INTERVAL = 500  # ms

def send_command(cmd):
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(1)
        sock.connect(SOCKET_PATH)
        sock.sendall((cmd + "\n").encode())
        data = b""
        while True:
            try:
                chunk = sock.recv(1024)
                if not chunk:
                    break
                data += chunk
            except socket.timeout:
                break
        sock.close()
        return data.decode()
    except Exception:
        return None

def update_list_async():
    raw = send_command("LIST")
    if not raw or raw.strip() == "":
        return
    rows = [line.split() for line in raw.splitlines() if line and line != 'END']

    sel = tree.selection()
    selected = tree.item(sel[0])['values'][0] if sel else None

    def ui():
        tree.delete(*tree.get_children())
        for pid, state, prio in rows:
            display_state = "Running" if state.lower() == "sleeping" else state
            display_tag = "running" if state.lower() == "sleeping" else state.lower()
            tree.insert('', 'end', values=(pid, display_state, prio), tags=(display_tag,))
        if selected:
            for iid in tree.get_children():
                if tree.item(iid)['values'][0] == selected:
                    tree.selection_set(iid)
                    break
        status_var.set(f"Processes: {len(rows)}")

    root.after(0, ui)

def schedule_update():
    threading.Thread(target=update_list_async, daemon=True).start()
    root.after(REFRESH_INTERVAL, schedule_update)

def get_selected_pid():
    sel = tree.selection()
    if not sel:
        messagebox.showwarning("Select PID", "Please select a process.")
        return None
    return tree.item(sel[0])['values'][0]

def pause_proc():
    pid = get_selected_pid()
    if pid:
        threading.Thread(target=lambda: send_command(f"PAUSE {pid}"), daemon=True).start()

def resume_proc():
    pid = get_selected_pid()
    if pid:
        threading.Thread(target=lambda: send_command(f"RESUME {pid}"), daemon=True).start()

def kill_proc():
    pid = get_selected_pid()
    if pid:
        threading.Thread(target=lambda: send_command(f"KILL {pid}"), daemon=True).start()

def set_priority():
    pid = get_selected_pid()
    pr = prio_entry.get()
    if pid and pr.isdigit():
        threading.Thread(target=lambda: send_command(f"PRIORITY {pid} {pr}"), daemon=True).start()
    else:
        messagebox.showwarning("Priority", "Enter a valid integer priority.")

def show_context_menu(event):
    iid = tree.identify_row(event.y)
    if iid:
        tree.selection_set(iid)
        menu.post(event.x_root, event.y_root)

# ------------------------- UI -------------------------------

root = tk.Tk()
root.title("Mini Task Manager")
root.geometry("600x450")
root.configure(bg="#1c1c1c")

style = ttk.Style()
style.theme_use("clam")
style.configure("Treeview",
                background="#262626",
                foreground="white",
                fieldbackground="#262626",
                rowheight=26,
                font=('Consolas', 11))
style.map("Treeview",
          background=[('selected', '#3a3a3a')],
          foreground=[('selected', 'white')])

frame = ttk.Frame(root, padding=10)
frame.pack(fill='both', expand=True)

tree = ttk.Treeview(frame, columns=('PID', 'State', 'Priority'), show='headings')
for col in ('PID', 'State', 'Priority'):
    tree.heading(col, text=col, anchor='center')
    tree.column(col, anchor='center', width=150)

# Tag colors
tree.tag_configure('running', background='#006400')      # dark green for running
tree.tag_configure('sleeping', background='#333333')      # gray for original sleeping (won't be used now)
tree.tag_configure('stopped', background='#8B8000')       # darker yellow
tree.tag_configure('zombie', background='#8B0000')        # dark red
tree.tag_configure('unknown', background='#555555')       # dark gray
tree.pack(fill='both', expand=True)

menu = tk.Menu(root, tearoff=0)
menu.add_command(label="Pause",  command=pause_proc)
menu.add_command(label="Resume", command=resume_proc)
menu.add_command(label="Kill",   command=kill_proc)
menu.add_separator()
menu.add_command(label="Set Priority", command=lambda: prio_entry.focus_set())
tree.bind("<Button-3>", show_context_menu)

btn_frame = ttk.Frame(root, padding=8)
btn_frame.pack(fill='x')
for text, cmd in [("Pause", pause_proc), ("Resume", resume_proc), ("Kill", kill_proc)]:
    ttk.Button(btn_frame, text=text, command=cmd).pack(side='left', padx=8)
prio_entry = ttk.Entry(btn_frame, width=5)
prio_entry.pack(side='left', padx=8)
prio_entry.insert(0, '0')
ttk.Button(btn_frame, text="Set Priority", command=set_priority).pack(side='left', padx=8)

status_var = tk.StringVar(value="Connecting to manager...")
status = ttk.Label(root, textvariable=status_var, anchor='w',
                   background="#1c1c1c", foreground='white', padding=6,
                   font=('Consolas', 10))
status.pack(fill='x', side='bottom')

root.after(100, schedule_update)
root.mainloop()
