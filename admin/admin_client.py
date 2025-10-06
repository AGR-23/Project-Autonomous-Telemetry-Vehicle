import customtkinter as ctk
from datetime import datetime
import threading
import time
import socket

class TelemetryInterface(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.server_socket = None
        self.host = "127.0.0.1"
        self.port = 9000
        self.is_connected = False
        self.receive_thread = None
        self.should_receive = False
        
        # Configure window
        self.title("Vehicle Telemetry System")
        self.geometry("900x700")
        
        # Set theme
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")
        
        # Initialize telemetry data
        self.telemetry_data = {
            "direction": "N",
            "speed": "0.0",
            "battery": "0.0",
            "temperature": "0.0",
            "time": datetime.now().strftime("%H:%M:%S")
        }
        
        # Create main layout
        self.create_layout()
        
        # Handle window close
        self.protocol("WM_DELETE_WINDOW", self.on_closing)

    def send_command(self, message):
        """Send command to server with error handling"""
        if not self.is_connected or self.server_socket is None:
            self.log_command("ERROR: Not connected to server")
            return False
            
        print(f"Sending: {message}")
        if not message.endswith('\n'):
            message += '\n'
        try:
            self.server_socket.send(message.encode())
            return True
        except Exception as e:
            self.log_command(f"ERROR sending command: {e}")
            self.disconnect()
            return False

    def connect(self):
        """Connect to server with proper state management"""
        if self.is_connected:
            self.log_command("Already connected to server")
            return
        
        try:
            # Create new socket
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.settimeout(5)  # 5 second timeout for connection
            
            self.log_command(f"Connecting to {self.host}:{self.port}...")
            self.server_socket.connect((self.host, self.port))
            
            # Remove timeout for normal operations
            self.server_socket.settimeout(None)
            
            # Authenticate
            self.server_socket.send("AUTH admin admin123\n".encode())
            time.sleep(0.1)
            
            # Update connection state
            self.is_connected = True
            self.status_label.configure(text="● Connected", text_color="green")
            self.connect_btn.configure(state="disabled")
            self.disconnect_btn.configure(state="normal")
            
            self.log_command("Successfully connected to server")
            
            # Start receiving data
            self.start_receive_thread()
            
        except socket.timeout:
            self.log_command("ERROR: Connection timeout")
            self.cleanup_socket()
        except ConnectionRefusedError:
            self.log_command("ERROR: Connection refused. Is the server running?")
            self.cleanup_socket()
        except Exception as e:
            self.log_command(f"ERROR connecting: {e}")
            self.cleanup_socket()

    def disconnect(self):
        """Properly disconnect from server"""
        if not self.is_connected:
            self.log_command("Already disconnected")
            return
        
        self.log_command("Disconnecting from server...")
        
        # Stop receive thread
        self.should_receive = False
        
        # Send QUIT command if possible
        try:
            if self.server_socket:
                self.server_socket.send("QUIT\n".encode())
                time.sleep(0.1)
        except:
            pass
        
        # Cleanup
        self.cleanup_socket()
        
        # Update UI
        self.is_connected = False
        self.status_label.configure(text="● Disconnected", text_color="red")
        self.connect_btn.configure(state="normal")
        self.disconnect_btn.configure(state="disabled")
        
        self.log_command("Disconnected from server")

    def cleanup_socket(self):
        """Clean up socket resources"""
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
            self.server_socket = None

    def start_receive_thread(self):
        """Start thread to receive telemetry data"""
        self.should_receive = True
        self.receive_thread = threading.Thread(target=self.receive_loop, daemon=True)
        self.receive_thread.start()

    def receive_loop(self):
        """Receive data from server in background thread"""
        buffer = ""
        while self.should_receive and self.is_connected:
            try:
                data = self.server_socket.recv(1024).decode()
                
                if not data:
                    # Server closed connection
                    self.after(0, lambda: self.log_command("Server closed connection"))
                    self.after(0, self.disconnect)
                    break
                
                buffer += data
                
                # Process complete messages
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    
                    if line and 'TLM' in line:
                        # Parse telemetry: TLM speed=10;battery=85;ts=12:30:45;temp=45;dir=N
                        telemetry_str = line.replace("TLM ", "")
                        telemetry = dict([x.split("=") for x in telemetry_str.split(";")])
                        
                        # Update telemetry data
                        self.telemetry_data["speed"] = telemetry.get("speed", "0.0")
                        self.telemetry_data["battery"] = telemetry.get("battery", "0.0")
                        self.telemetry_data["time"] = telemetry.get("ts", datetime.now().strftime("%H:%M:%S"))
                        self.telemetry_data["temperature"] = telemetry.get("temp", "0.0")
                        self.telemetry_data["direction"] = telemetry.get("dir", "N")

                        self.log_command("Telemetry received")
                        
                        # Update UI on main thread
                        self.after(0, self.update_telemetry_display)
                    elif line and 'dir' in line:
                        direction = line.strip("OK ").split("=")[1]
                        self.telemetry_data["direction"] = direction
                        self.update_telemetry_display()

                    elif line and 'speed' in line:
                        speed = line.strip("OK ").split("=")[1]
                        self.telemetry_data["speed"] = speed
                        self.update_telemetry_display()
                    
                    elif line:
                        # Log other server responses
                        self.after(0, lambda msg=line: self.log_command(f"Server: {msg}"))
                        
            except Exception as e:
                if self.should_receive:
                    self.after(0, lambda: self.log_command(f"ERROR receiving data: {e}"))
                    self.after(0, self.disconnect)
                break

    def on_closing(self):
        """Handle window close event"""
        if self.is_connected:
            self.disconnect()
        time.sleep(0.2)  # Give time for cleanup
        self.destroy()
    
    def create_layout(self):
        # Main container with padding
        main_frame = ctk.CTkFrame(self, fg_color="transparent")
        main_frame.pack(fill="both", expand=True, padx=20, pady=20)

        # Title
        title_label = ctk.CTkLabel(
            main_frame, 
            text="Vehicle Telemetry Dashboard",
            font=ctk.CTkFont(size=28, weight="bold")
        )
        title_label.pack(pady=(0, 20))
        
        # Create two columns
        content_frame = ctk.CTkFrame(main_frame, fg_color="transparent")
        content_frame.pack(fill="both", expand=True)
        
        # Left column - Telemetry Display
        self.create_telemetry_display(content_frame)
        
        # Right column - Control Panel
        self.create_control_panel(content_frame)
    
    def create_telemetry_display(self, parent):
        telemetry_frame = ctk.CTkFrame(parent)
        telemetry_frame.pack(side="left", fill="both", expand=True, padx=(0, 10))
        
        # Section title
        ctk.CTkLabel(
            telemetry_frame,
            text="Telemetry Data",
            font=ctk.CTkFont(size=20, weight="bold")
        ).pack(pady=(15, 20))
        
        # Time display
        time_frame = ctk.CTkFrame(telemetry_frame)
        time_frame.pack(fill="x", padx=15, pady=5)
        ctk.CTkLabel(time_frame, text="Time:", font=ctk.CTkFont(size=14, weight="bold")).pack(side="left", padx=10, pady=10)
        self.time_label = ctk.CTkLabel(time_frame, text="00:00:00", font=ctk.CTkFont(size=14))
        self.time_label.pack(side="right", padx=10, pady=10)
        
        # Direction/Compass display
        direction_frame = ctk.CTkFrame(telemetry_frame)
        direction_frame.pack(fill="x", padx=15, pady=10)
        ctk.CTkLabel(
            direction_frame, 
            text="Direction", 
            font=ctk.CTkFont(size=16, weight="bold")
        ).pack(pady=(10, 15))
        
        # Compass visualization
        compass_container = ctk.CTkFrame(direction_frame, fg_color="transparent")
        compass_container.pack(pady=10)
        
        # Create compass grid
        compass_grid = ctk.CTkFrame(compass_container, fg_color="transparent")
        compass_grid.pack()
        
        # North (top)
        self.north_indicator = ctk.CTkLabel(
            compass_grid, 
            text="N", 
            font=ctk.CTkFont(size=20, weight="bold"),
            width=50,
            height=50,
            fg_color="gray25",
            corner_radius=10
        )
        self.north_indicator.grid(row=0, column=1, padx=5, pady=5)
        
        # West (left), Center, East (right)
        self.west_indicator = ctk.CTkLabel(
            compass_grid, 
            text="W", 
            font=ctk.CTkFont(size=20, weight="bold"),
            width=50,
            height=50,
            fg_color="gray25",
            corner_radius=10
        )
        self.west_indicator.grid(row=1, column=0, padx=5, pady=5)
        
        # Center indicator (current direction)
        self.direction_label = ctk.CTkLabel(
            compass_grid, 
            text="—", 
            font=ctk.CTkFont(size=24, weight="bold"),
            width=50,
            height=50,
            fg_color="blue",
            corner_radius=10,
            text_color="white"
        )
        self.direction_label.grid(row=1, column=1, padx=5, pady=5)
        
        self.east_indicator = ctk.CTkLabel(
            compass_grid, 
            text="E", 
            font=ctk.CTkFont(size=20, weight="bold"),
            width=50,
            height=50,
            fg_color="gray25",
            corner_radius=10
        )
        self.east_indicator.grid(row=1, column=2, padx=5, pady=5)
        
        # South (bottom)
        self.south_indicator = ctk.CTkLabel(
            compass_grid, 
            text="S", 
            font=ctk.CTkFont(size=20, weight="bold"),
            width=50,
            height=50,
            fg_color="gray25",
            corner_radius=10
        )
        self.south_indicator.grid(row=2, column=1, padx=5, pady=5)
        
        # Speed display
        speed_frame = ctk.CTkFrame(telemetry_frame)
        speed_frame.pack(fill="x", padx=15, pady=5)
        ctk.CTkLabel(speed_frame, text="Speed:", font=ctk.CTkFont(size=14, weight="bold")).pack(side="left", padx=10, pady=10)
        self.speed_label = ctk.CTkLabel(speed_frame, text="0.0 km/h", font=ctk.CTkFont(size=14))
        self.speed_label.pack(side="right", padx=10, pady=10)
        
        # Battery display
        battery_frame = ctk.CTkFrame(telemetry_frame)
        battery_frame.pack(fill="x", padx=15, pady=5)
        ctk.CTkLabel(battery_frame, text="🔋 Battery:", font=ctk.CTkFont(size=14, weight="bold")).pack(side="left", padx=10, pady=10)
        self.battery_label = ctk.CTkLabel(battery_frame, text=self.telemetry_data["battery"], font=ctk.CTkFont(size=14))
        self.battery_label.pack(side="right", padx=10, pady=10)
        
        self.battery_bar = ctk.CTkProgressBar(telemetry_frame)
        self.battery_bar.pack(fill="x", padx=25, pady=(0, 10))
        self.battery_bar.set(1.0)
        
        # Temperature display
        temp_frame = ctk.CTkFrame(telemetry_frame)
        temp_frame.pack(fill="x", padx=15, pady=5)
        ctk.CTkLabel(temp_frame, text="Temperature:", font=ctk.CTkFont(size=14, weight="bold")).pack(side="left", padx=10, pady=10)
        self.temp_label = ctk.CTkLabel(temp_frame, text=self.telemetry_data["temperature"], font=ctk.CTkFont(size=14))
        self.temp_label.pack(side="right", padx=10, pady=10)
        
        # Connection status
        status_frame = ctk.CTkFrame(telemetry_frame)
        status_frame.pack(fill="x", padx=15, pady=(15, 10))
        ctk.CTkLabel(status_frame, text="Status:", font=ctk.CTkFont(size=14, weight="bold")).pack(side="left", padx=10, pady=10)
        self.status_label = ctk.CTkLabel(status_frame, text="● Disconnected", font=ctk.CTkFont(size=14), text_color="red")
        self.status_label.pack(side="right", padx=10, pady=10)
    
    def create_control_panel(self, parent):
        control_frame = ctk.CTkFrame(parent)
        control_frame.pack(side="right", fill="both", expand=True, padx=(10, 0))
        
        # Section title
        ctk.CTkLabel(
            control_frame,
            text="Control Panel",
            font=ctk.CTkFont(size=20, weight="bold")
        ).pack(pady=(15, 20))
        
        # Speed controls
        speed_section = ctk.CTkFrame(control_frame)
        speed_section.pack(fill="x", padx=15, pady=10)
        
        ctk.CTkLabel(
            speed_section,
            text="Speed Control",
            font=ctk.CTkFont(size=16, weight="bold")
        ).pack(pady=(10, 15))
        
        speed_up_btn = ctk.CTkButton(
            speed_section,
            text="Speed Up",
            command=self.speed_up,
            height=40,
            font=ctk.CTkFont(size=14)
        )
        speed_up_btn.pack(fill="x", padx=20, pady=5)
        
        slow_down_btn = ctk.CTkButton(
            speed_section,
            text="Slow Down",
            command=self.slow_down,
            height=40,
            font=ctk.CTkFont(size=14)
        )
        slow_down_btn.pack(fill="x", padx=20, pady=(5, 15))
        
        # Direction controls
        direction_section = ctk.CTkFrame(control_frame)
        direction_section.pack(fill="x", padx=15, pady=10)  
        
        ctk.CTkLabel(
            direction_section,
            text="Direction Control",
            font=ctk.CTkFont(size=16, weight="bold")
        ).pack(pady=(10, 10))
        
        # Left and Right buttons in a row
        lr_frame = ctk.CTkFrame(direction_section, fg_color="transparent")
        lr_frame.pack(fill="x", padx=20, pady=5)
        
        left_btn = ctk.CTkButton(
            lr_frame,
            text="Turn Left",
            command=lambda: self.change_direction("LEFT"),
            height=40,
            font=ctk.CTkFont(size=14)
        )
        left_btn.pack(side="left", fill="x", expand=True, padx=(0, 5))
        
        right_btn = ctk.CTkButton(
            lr_frame,
            text="Turn Right",
            command=lambda: self.change_direction("RIGHT"),
            height=40,
            font=ctk.CTkFont(size=14)
        )
        right_btn.pack(side="right", fill="x", expand=True, padx=(5, 0))
        
        # Connection controls
        connection_section = ctk.CTkFrame(control_frame)
        connection_section.pack(fill="x", padx=15, pady=10)
        
        self.connect_btn = ctk.CTkButton(
            connection_section,
            text="CONNECT",
            command=self.connect,
            height=50,
            font=ctk.CTkFont(size=16, weight="bold"),
            fg_color="green",
            hover_color="darkgreen"
        )
        self.connect_btn.pack(fill="x", padx=20, pady=5)
        
        self.disconnect_btn = ctk.CTkButton(
            connection_section,
            text="DISCONNECT",
            command=self.disconnect,
            height=50,
            font=ctk.CTkFont(size=16, weight="bold"),
            fg_color="red",
            hover_color="darkred",
            state="disabled"
        )
        self.disconnect_btn.pack(fill="x", padx=20, pady=5)
        
        # Log display
        log_section = ctk.CTkFrame(control_frame)
        log_section.pack(fill="both", expand=True, padx=15, pady=10)
        
        ctk.CTkLabel(
            log_section,
            text="Command Log",
            font=ctk.CTkFont(size=14, weight="bold")
        ).pack(pady=(10, 5))
        
        self.log_text = ctk.CTkTextbox(log_section, height=150, font=ctk.CTkFont(size=12))
        self.log_text.pack(fill="both", expand=True, padx=10, pady=(5, 10))
    
    def update_telemetry_display(self):
        """Update all telemetry displays with current data"""
        # Update time
        self.time_label.configure(text=self.telemetry_data["time"])
        
        # Update direction compass
        direction = self.telemetry_data["direction"]
        self.direction_label.configure(text=direction)
        
        # Reset all indicators
        self.north_indicator.configure(fg_color="gray25")
        self.south_indicator.configure(fg_color="gray25")
        self.east_indicator.configure(fg_color="gray25")
        self.west_indicator.configure(fg_color="gray25")
        
        # Highlight current direction
        if direction == "N":
            self.north_indicator.configure(fg_color="green")
        elif direction == "S":
            self.south_indicator.configure(fg_color="green")
        elif direction == "E":
            self.east_indicator.configure(fg_color="green")
        elif direction == "W":
            self.west_indicator.configure(fg_color="green")
        
        # Update speed
        self.speed_label.configure(text=f"{self.telemetry_data['speed']} km/h")
        
        # Update battery
        try:
            battery = float(self.telemetry_data['battery'])
            self.battery_label.configure(text=f"{battery:.0f}%")
            self.battery_bar.set(battery / 100)
            
            # Change battery color based on level
            if battery < 20:
                self.battery_label.configure(text_color="red")
            elif battery < 50:
                self.battery_label.configure(text_color="orange")
            else:
                self.battery_label.configure(text_color="green")
        except:
            pass
        
        # Update temperature
        try:
            temp = float(self.telemetry_data['temperature'])
            self.temp_label.configure(text=f"{temp:.1f}°C")
            
            # Change temperature color based on value
            if temp > 80:
                self.temp_label.configure(text_color="red")
            elif temp > 60:
                self.temp_label.configure(text_color="orange")
            else:
                self.temp_label.configure(text_color="white")
        except:
            pass
    
    def log_command(self, message):
        """Add a command to the log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{timestamp}] {message}\n")
        self.log_text.see("end")
    
    # Command functions
    def speed_up(self):
        if self.send_command("SPEED UP"):
            self.log_command("Command sent: SPEED UP")
    
    def slow_down(self):
        if self.send_command("SLOW DOWN"):
            self.log_command("Command sent: SLOW DOWN")
    
    def change_direction(self, direction):
        if self.send_command("TURN " + direction):
            self.log_command(f"Command sent: TURN {direction}")

if __name__ == "__main__":
    app = TelemetryInterface()
    app.mainloop()