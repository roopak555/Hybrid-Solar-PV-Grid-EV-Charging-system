import tkinter as tk
import math
import serial
import random

BG_COLOR = "#0A0A0A"
LED_GREEN = "#00FF41" 
GOLD = "#F1C40F"      
ELECTRIC_BLUE = "#3498DB" 
WARNING_RED = "#E74C3C"  
WHITE = "#FFFFFF"
PURPLE = "#8E44AD"

class EVChargingUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.geometry("480x320")
        self.attributes('-fullscreen', True)
        self.configure(bg=BG_COLOR)
        
        # --- THE SOFTWARE CURSOR HACK ---
        self.fake_cursor = tk.Label(self, text="+", font=("Arial", 35, "bold"), fg="red", bg=BG_COLOR)
        self.fake_cursor.place(x=240, y=160)
        self.bind('<Motion>', self.move_fake_cursor)
        
        self.state = "BOOT" 
        self.charge_amount = 0.0
        
        self.hybrid_state = 2  # 0=OFF, 1=FORCED, 2=AUTO
        
        # --- UI Display Trackers ---
        self.battery_type = "LEAD"  
        self.show_details = False   
        
        # --- Animation & Smoothing Variables ---
        self.target_percent = 0     
        self.display_percent = 0    
        
        # --- Serial Port Setup ---
        try:
            self.ser = serial.Serial('/dev/serial0', 115200, timeout=0.1)
            self.uart_active = True
        except Exception as e:
            self.uart_active = False
            print("UART ERROR: Could not open /dev/serial0")
        
        self.container = tk.Frame(self, bg=BG_COLOR)
        self.container.pack(fill="both", expand=True)
        self.container.grid_rowconfigure(0, weight=1)
        self.container.grid_columnconfigure(0, weight=1)
        
        self.page1_welcome = tk.Frame(self.container, bg=BG_COLOR)
        self.page2_connect = tk.Frame(self.container, bg=BG_COLOR)
        self.page3_dashboard = tk.Frame(self.container, bg=BG_COLOR)
        self.page4_billing = tk.Frame(self.container, bg=BG_COLOR)
        
        for frame in (self.page1_welcome, self.page2_connect, self.page3_dashboard, self.page4_billing):
            frame.grid(row=0, column=0, sticky="nsew")
            
        # ==========================================
        # PAGE 1: WELCOME SCREEN
        # ==========================================
        self.logo_canvas = tk.Canvas(self.page1_welcome, width=150, height=120, bg=BG_COLOR, highlightthickness=0)
        self.logo_canvas.pack(pady=(50, 10))
        self.bolt_points = [(15, -30), (5, -5), (30, -5), (-15, 30), (-5, 5), (-30, 5)]
        self.bolt = self.logo_canvas.create_polygon([0]*12, fill=LED_GREEN, outline=LED_GREEN, width=2)
        self.bolt_scale = 0.1
        self.bolt_alpha = 0

        self.title_label = tk.Label(self.page1_welcome, text="EV CHARGING STATION", font=("Helvetica", 22, "bold"), fg=BG_COLOR, bg=BG_COLOR)
        self.title_label.pack(pady=15)
        
        if not self.uart_active:
            tk.Label(self.page1_welcome, text="UART OFFLINE - CHECK PINS", font=("Arial", 10), fg=WARNING_RED, bg=BG_COLOR).pack()
        
        # ==========================================
        # PAGE 2: BATTERY CONNECTION
        # ==========================================
        tk.Label(self.page2_connect, text="SYSTEM STATUS", font=("Helvetica", 20, "bold"), fg=WHITE, bg=BG_COLOR).pack(pady=(15, 5))
        self.batt_canvas = tk.Canvas(self.page2_connect, width=120, height=60, bg=BG_COLOR, highlightthickness=0)
        self.batt_canvas.pack(pady=5)
        self.status_label = tk.Label(self.page2_connect, text="PLEASE CONNECT THE BATTERY...", font=("Helvetica", 16, "bold"), fg=WARNING_RED, bg=BG_COLOR)
        self.status_label.pack(pady=5)
        
        self.btn_frame = tk.Frame(self.page2_connect, bg=BG_COLOR)
        self.lead_btn = tk.Button(self.btn_frame, text="LEAD-ACID (14.4V)", font=("Helvetica", 12, "bold"), bg="gray", fg="white", width=16, command=self.select_lead_acid)
        self.lead_btn.grid(row=0, column=0, padx=10)
        self.lion_btn = tk.Button(self.btn_frame, text="LITHIUM-ION (12.6V)", font=("Helvetica", 12, "bold"), bg="gray", fg="white", width=16, command=self.select_lithium)
        self.lion_btn.grid(row=0, column=1, padx=10)
        
        self.start_btn = tk.Button(self.page2_connect, text="START CHARGING ⚡", font=("Helvetica", 16, "bold"), bg=LED_GREEN, fg="black", width=18, command=self.go_to_dashboard)
        
        # ==========================================
        # PAGE 3: LIVE DASHBOARD SCREEN
        # ==========================================
        self.dash_header = tk.Label(self.page3_dashboard, text="⚡ LIVE CHARGING DASHBOARD ⚡", font=("Helvetica", 16, "bold"), fg="black", bg=GOLD)
        self.dash_header.pack(fill="x", ipady=5)
        
        self.toggle_view_btn = tk.Button(self.page3_dashboard, text=" 🔄 V/A ", font=("Helvetica", 10, "bold"), bg="#444444", fg="white", command=self.toggle_view)
        self.toggle_view_btn.place(x=400, y=50)

        self.hybrid_btn = tk.Button(self.page3_dashboard, text="[ Hybrid Mode: AUTO ]", font=("Helvetica", 12, "bold"), bg=ELECTRIC_BLUE, fg="white", command=self.toggle_hybrid_mode)
        self.hybrid_btn.pack(pady=(5, 0))

        # Main Info Frame
        self.info_frame = tk.Frame(self.page3_dashboard, bg=BG_COLOR)
        self.info_frame.pack(expand=True, fill="both", pady=5)
        
        self.percent_label = tk.Label(self.info_frame, text="0%", font=("Helvetica", 65, "bold"), fg=LED_GREEN, bg=BG_COLOR)
        self.percent_label.pack(expand=True)
        
        self.details_frame = tk.Frame(self.info_frame, bg=BG_COLOR)
        self.volt_label = tk.Label(self.details_frame, text="0.0 V", font=("Helvetica", 34, "bold"), fg="yellow", bg=BG_COLOR)
        self.volt_label.grid(row=0, column=0, padx=25, pady=10)
        self.amp_label = tk.Label(self.details_frame, text="0.0 A", font=("Helvetica", 34, "bold"), fg="cyan", bg=BG_COLOR)
        self.amp_label.grid(row=0, column=1, padx=25, pady=10)
        
        self.source_label = tk.Label(self.page3_dashboard, text="SOURCE: --", font=("Helvetica", 18, "bold"), fg="white", bg=BG_COLOR)
        self.source_label.pack(pady=5)

        tk.Button(self.page3_dashboard, text="DISCONNECT BATTERY", font=("Helvetica", 14, "bold"), bg=WARNING_RED, fg="white", command=self.generate_bill).pack(pady=(0, 10))
        
        # ==========================================
        # PAGE 4: BILLING
        # ==========================================
        tk.Label(self.page4_billing, text="PAYMENT INVOICE", font=("Helvetica", 18, "bold"), fg="black", bg=WHITE).pack(fill="x", ipady=5)
        self.bill_amount_label = tk.Label(self.page4_billing, text="Total Due: ₹ 0.00", font=("Helvetica", 22, "bold"), fg=GOLD, bg=BG_COLOR)
        self.bill_amount_label.pack(pady=10)
        self.qr_canvas = tk.Canvas(self.page4_billing, width=120, height=120, bg="white", highlightthickness=0)
        self.qr_canvas.pack(pady=5)
        self.pay_status_label = tk.Label(self.page4_billing, text="Scan QR to Pay", font=("Helvetica", 14), fg="white", bg=BG_COLOR)
        self.pay_status_label.pack(pady=5)
        self.pay_btn = tk.Button(self.page4_billing, text="[SIMULATE PAYMENT]", font=("Helvetica", 12, "bold"), bg=ELECTRIC_BLUE, fg="white", command=self.process_payment)
        self.pay_btn.pack(pady=5)

        tk.Button(self, text="X", font=("Arial", 10), bg="red", fg="white", command=self.destroy).place(x=450, y=0)
        
        self.show_frame(self.page1_welcome)
        self.after(500, self.animate_logo_intro)
        self.after(1000, self.read_uart_data) 
        
        # Start background loop
        self.after(50, self.animate_percentage_tick)
        
    def show_frame(self, frame):
        frame.tkraise()

    def move_fake_cursor(self, event):
        self.fake_cursor.place(x=event.x_root + 2, y=event.y_root + 2)
        self.fake_cursor.lift() 

    def animate_percentage_tick(self):
        if self.state == "DASHBOARD" and not self.show_details:
            if self.display_percent < self.target_percent:
                self.display_percent += 1
                self.percent_label.config(text=f"{self.display_percent}%")
        self.after(50, self.animate_percentage_tick)

    def toggle_view(self):
        self.show_details = not self.show_details
        if self.show_details:
            self.percent_label.pack_forget()
            self.details_frame.pack(expand=True)
            self.toggle_view_btn.config(text=" 🔄 % ")
        else:
            self.details_frame.pack_forget()
            self.percent_label.config(text=f"{self.display_percent}%")
            self.percent_label.pack(expand=True)
            self.toggle_view_btn.config(text=" 🔄 V/A ")

    def calculate_percentage(self, volts):
        if self.battery_type == "LEAD":
            min_v, max_v = 11.5, 14.4
        else: 
            min_v, max_v = 9.6, 12.6
            
        # --- FIX 2: THE CALIBRATION TRICK ---
        # Never returns 100 based on voltage. Caps at 99.
        if volts >= max_v: return 99
        if volts <= min_v: return 0
        
        pct = ((volts - min_v) / (max_v - min_v)) * 100
        if pct >= 99: return 99 
        return int(pct)

    def select_lead_acid(self):
        self.battery_type = "LEAD" 
        if self.uart_active:
            try: self.ser.write(b"LEAD\n")
            except: pass
        self.lead_btn.config(bg=ELECTRIC_BLUE)
        self.lion_btn.config(bg="gray")
        self.start_btn.pack(pady=15)

    def select_lithium(self):
        self.battery_type = "LION"
        if self.uart_active:
            try: self.ser.write(b"LION\n")
            except: pass
        self.lion_btn.config(bg=PURPLE)
        self.lead_btn.config(bg="gray")
        self.start_btn.pack(pady=15)

    def toggle_hybrid_mode(self):
        self.hybrid_state = (self.hybrid_state + 1) % 3
        if self.hybrid_state == 0:
            if self.uart_active:
                try: self.ser.write(b"SCHED_OFF\n")
                except: pass
            self.hybrid_btn.config(text="[ Hybrid Mode: OFF ]", bg="gray")
        elif self.hybrid_state == 1:
            if self.uart_active:
                try: self.ser.write(b"SCHED_ON\n")
                except: pass
            self.hybrid_btn.config(text="[ Hybrid Mode: FORCED ]", bg=PURPLE)
        elif self.hybrid_state == 2:
            if self.uart_active:
                try: self.ser.write(b"SCHED_AUTO\n")
                except: pass
            self.hybrid_btn.config(text="[ Hybrid Mode: AUTO ]", bg=ELECTRIC_BLUE)

    def read_uart_data(self):
        if self.uart_active and self.ser.in_waiting > 0:
            try:
                raw_line = self.ser.readline().decode('utf-8').strip()
                if raw_line.startswith("BATT:"):
                    parts = raw_line.split(',')
                    data_dict = {}
                    for part in parts:
                        if ':' in part:
                            key, val = part.split(':')
                            data_dict[key] = val
                    
                    if data_dict.get("BATT") == "0":
                        if self.state not in ["WAITING_FOR_BATT", "BOOT"]:
                            self.handle_battery_disconnected()
                            
                    elif data_dict.get("BATT") == "1":
                        if self.state == "WAITING_FOR_BATT":
                            self.handle_battery_connected()
                        elif self.state == "DASHBOARD":
                            self.update_dashboard_values(data_dict)
            except Exception as e: pass 
        self.after(50, self.read_uart_data)

    def handle_battery_disconnected(self):
        self.state = "WAITING_FOR_BATT"
        self.draw_battery("empty")
        self.status_label.config(text="PLEASE CONNECT THE BATTERY...", fg=WARNING_RED)
        self.btn_frame.pack_forget()
        self.start_btn.pack_forget()
        
        # --- FIX 1: MEMORY HELD ---
        # The variables self.target_percent and display_percent are NO LONGER reset here!
        # This ignores the brief voltage drop when the relays click.
        
        self.hybrid_state = 2
        self.hybrid_btn.config(text="[ Hybrid Mode: AUTO ]", bg=ELECTRIC_BLUE)
        self.show_frame(self.page2_connect)

    def handle_battery_connected(self):
        self.state = "BATT_CONNECTED"
        self.draw_battery("full")
        self.status_label.config(text="SELECT BATTERY CHEMISTRY:", fg=LED_GREEN)
        
        # (Variables are also NOT reset here)
        
        self.lead_btn.config(bg="gray")
        self.lion_btn.config(bg="gray")
        self.btn_frame.pack(pady=10)
        
        self.qr_canvas.pack(pady=5)
        self.pay_status_label.config(text="Scan QR to Pay", fg="white", font=("Helvetica", 14))
        self.pay_btn.pack(pady=5)

    def update_dashboard_values(self, data):
        try:
            volts = float(data.get("VOLT", "0.00"))
            amps = float(data.get("AMP", "0.00"))
            dc_amps = float(data.get("DC", "0.00"))
            ac_amps = float(data.get("AC", "0.00"))
        except ValueError:
            volts, amps, dc_amps, ac_amps = 0.0, 0.0, 0.0, 0.0
            
        source = data.get("SRC", "UNKNOWN")
        
        # This ensures the percentage only goes up
        raw_pct = self.calculate_percentage(volts)
        if raw_pct > self.target_percent:
            self.target_percent = raw_pct
            
        self.volt_label.config(text=f"{volts:.1f} V")
        
        if source in ["PV", "GRID", "HYBRID"]: 
            self.charge_amount += 0.50
            
        if source == "FULL":
            # The ONLY way to reach 100% is when STM32 says tail current is 0.6A
            self.target_percent = 100
            self.display_percent = 100
            self.percent_label.config(text="100%")
            self.amp_label.config(text="0.0 A", font=("Helvetica", 34, "bold"))
            self.source_label.config(text="FULLY CHARGED", fg=WHITE)
            self.generate_bill() 
        else:
            if source == "PV":
                self.amp_label.config(text=f"{amps:.1f} A", font=("Helvetica", 34, "bold"))
                self.source_label.config(text="ACTIVE SOURCE: PV SYSTEM", fg=LED_GREEN)
            elif source == "GRID":
                self.amp_label.config(text=f"{amps:.1f} A", font=("Helvetica", 34, "bold"))
                self.source_label.config(text="ACTIVE SOURCE: GRID NETWORK", fg=ELECTRIC_BLUE)
            elif source == "HYBRID":
                self.amp_label.config(text=f"PV: {dc_amps:.1f} A\nGRID: {ac_amps:.1f} A", font=("Helvetica", 24, "bold"))
                self.source_label.config(text="SOURCE: HYBRID (PV + GRID)", fg=PURPLE)

    def generate_bill(self):
        if self.state == "BILLING": return 
        self.state = "BILLING"
        self.show_frame(self.page4_billing)
        if self.uart_active:
            try: self.ser.write(b"DISCONNECT\n")
            except Exception as e: pass
        if self.charge_amount < 5.0: self.charge_amount = random.uniform(25.0, 75.0)
        self.bill_amount_label.config(text=f"Total Due: ₹ {self.charge_amount:.2f}")
        self.draw_fake_qr()

    def process_payment(self):
        self.state = "PAID"
        self.qr_canvas.pack_forget()
        self.pay_btn.pack_forget() 
        self.pay_status_label.config(
            text="✔\n\nPAYMENT SUCCESSFUL!\n\nPlease physically disconnect\nthe battery now.", 
            fg=LED_GREEN, font=("Helvetica", 18, "bold")
        )

    def draw_fake_qr(self):
        self.qr_canvas.delete("all")
        for r in range(12):
            for c in range(12):
                color = "black" if random.random() > 0.5 else "white"
                self.qr_canvas.create_rectangle(r*10, c*10, (r+1)*10, (c+1)*10, fill=color, outline=color)
        for x, y in [(0,0), (80,0), (0,80)]:
            self.qr_canvas.create_rectangle(x, y, x+40, y+40, fill="white", outline="")
            self.qr_canvas.create_rectangle(x+5, y+5, x+35, y+35, fill="black", outline="")
            self.qr_canvas.create_rectangle(x+10, y+10, x+30, y+30, fill="white", outline="")
            self.qr_canvas.create_rectangle(x+15, y+15, x+25, y+25, fill="black", outline="")

    def animate_logo_intro(self):
        if self.bolt_scale < 1.0: self.bolt_scale += 0.05
        if self.bolt_alpha < 255: self.bolt_alpha += 15
        scaled_points = []
        for x, y in self.bolt_points:
            scaled_points.append(x * self.bolt_scale + 75) 
            scaled_points.append(y * self.bolt_scale + 60) 
        current_color = f"#{int(self.bolt_alpha/255 * int(LED_GREEN[1:3], 16)):02x}{int(self.bolt_alpha/255 * int(LED_GREEN[3:5], 16)):02x}{int(self.bolt_alpha/255 * int(LED_GREEN[5:7], 16)):02x}"
        self.logo_canvas.coords(self.bolt, *scaled_points)
        self.logo_canvas.itemconfig(self.bolt, fill=current_color, outline=current_color)
        if self.bolt_scale < 1.0 or self.bolt_alpha < 255:
            self.after(30, self.animate_logo_intro)
        else:
            self.fade_in_text(self.title_label, WHITE, self.switch_to_connect_page)

    def fade_in_text(self, widget, target_color, callback=None):
        steps = 20
        start_rgb = tuple(int(BG_COLOR[i:i+2], 16) for i in (1, 3, 5))
        end_rgb = tuple(int(target_color[i:i+2], 16) for i in (1, 3, 5))
        for step in range(steps + 1):
            curr_rgb = [int(start_rgb[i] + (end_rgb[i] - start_rgb[i]) * step / steps) for i in range(3)]
            curr_color = f"#{curr_rgb[0]:02x}{curr_rgb[1]:02x}{curr_rgb[2]:02x}"
            self.after(step * 40, lambda c=curr_color: widget.config(fg=c))
        if callback: self.after((steps + 1) * 40 + 1500, callback) 

    def switch_to_connect_page(self):
        self.show_frame(self.page2_connect)
        self.state = "WAITING_FOR_BATT"
        self.draw_battery("empty")
        self.blink_status()
        
    def draw_battery(self, state):
        self.batt_canvas.delete("all")
        if state == "empty":
            self.batt_canvas.create_rectangle(15, 10, 95, 50, outline=WARNING_RED, width=4) 
            self.batt_canvas.create_rectangle(95, 20, 105, 40, fill=WARNING_RED, outline=WARNING_RED) 
            self.batt_canvas.create_line(40, 15, 70, 45, fill=WARNING_RED, width=4)
            self.batt_canvas.create_line(70, 15, 40, 45, fill=WARNING_RED, width=4)
        elif state == "full":
            self.batt_canvas.create_rectangle(15, 10, 95, 50, outline=LED_GREEN, width=4) 
            self.batt_canvas.create_rectangle(95, 20, 105, 40, fill=LED_GREEN, outline=LED_GREEN) 
            self.batt_canvas.create_rectangle(22, 16, 42, 44, fill=LED_GREEN, outline="")
            self.batt_canvas.create_rectangle(47, 16, 67, 44, fill=LED_GREEN, outline="")
            self.batt_canvas.create_rectangle(72, 16, 88, 44, fill=LED_GREEN, outline="")

    def blink_status(self):
        if self.state == "WAITING_FOR_BATT":
            current_color = self.status_label.cget("fg")
            next_color = BG_COLOR if current_color == WARNING_RED else WARNING_RED
            self.status_label.config(fg=next_color)
            self.after(500, self.blink_status)

    def go_to_dashboard(self):
        self.charge_amount = 0.0 
        
        # --- FIX 1 (Part 2): RESET WHEN NEW CHARGE STARTS ---
        self.target_percent = 0
        self.display_percent = 0
        self.percent_label.config(text="0%")
        
        self.hybrid_state = 2
        self.hybrid_btn.config(text="[ Hybrid Mode: AUTO ]", bg=ELECTRIC_BLUE)
        
        self.show_details = False
        self.details_frame.pack_forget()
        self.percent_label.pack(expand=True)
        self.toggle_view_btn.config(text=" 🔄 V/A ")
        
        if self.uart_active:
            try: 
                self.ser.write(b"START\n")
                self.ser.write(b"SCHED_AUTO\n") 
            except Exception as e: pass
                
        self.show_frame(self.page3_dashboard)
        self.state = "DASHBOARD"

if __name__ == "__main__":
    app = EVChargingUI() 
    app.mainloop()
