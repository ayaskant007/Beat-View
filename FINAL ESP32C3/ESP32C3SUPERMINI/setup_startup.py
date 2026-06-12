import os
import sys
import subprocess

def main():
    startup_dir = os.path.join(os.environ["APPDATA"], r"Microsoft\Windows\Start Menu\Programs\Startup")
    shortcut_path = os.path.join(startup_dir, "BeatViewMediaController.lnk")
    
    print("--- Beat-View Startup Shortcut Manager ---")
    print("1. Install shortcut (Run silently in background on Windows startup)")
    print("2. Uninstall shortcut (Stop running on startup)")
    choice = input("Select an option (1 or 2): ").strip()
    
    if choice == "1":
        # Determine paths
        script_dir = os.path.dirname(os.path.abspath(__file__))
        script_path = os.path.join(script_dir, "Final.py")
        
        # Check if venv pythonw.exe exists
        venv_pythonw = os.path.join(script_dir, ".venv", "Scripts", "pythonw.exe")
        if os.path.exists(venv_pythonw):
            python_exec = venv_pythonw
        else:
            # Fallback to current sys.executable directory's pythonw.exe
            current_dir = os.path.dirname(sys.executable)
            python_exec = os.path.join(current_dir, "pythonw.exe")
            if not os.path.exists(python_exec):
                # Final fallback to sys.executable
                python_exec = sys.executable
        
        print(f"\nTarget interpreter: {python_exec}")
        print(f"Target script: {script_path}")
        print(f"Working directory: {script_dir}")
        print(f"Shortcut location: {shortcut_path}")
        
        # PowerShell script to create the Windows shortcut
        ps_cmd = (
            f'$WshShell = New-Object -ComObject WScript.Shell; '
            f'$Shortcut = $WshShell.CreateShortcut("{shortcut_path}"); '
            f'$Shortcut.TargetPath = "{python_exec}"; '
            f'$Shortcut.Arguments = "`"{script_path}`""; '
            f'$Shortcut.WorkingDirectory = "{script_dir}"; '
            f'$Shortcut.Save()'
        )
        
        try:
            subprocess.run(["powershell", "-Command", ps_cmd], check=True)
            print("\n[SUCCESS] Windows startup shortcut created successfully!")
            print("The media controller will now run silently in the background whenever you log in.")
        except Exception as e:
            print(f"\n[ERROR] Failed to create shortcut: {e}")
            
    elif choice == "2":
        if os.path.exists(shortcut_path):
            try:
                os.remove(shortcut_path)
                print("\n[SUCCESS] Startup shortcut removed successfully!")
            except Exception as e:
                print(f"\n[ERROR] Failed to remove shortcut: {e}")
        else:
            print("\n[INFO] No startup shortcut was found to remove.")
    else:
        print("\nInvalid choice. Exiting.")

if __name__ == "__main__":
    main()
