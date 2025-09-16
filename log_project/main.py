from datetime import datetime
from pathlib import Path

class LogFileManager:
    def __init__(self):
        self.files = {}

    def openLogFile(self, filename):
        self.files[filename] = open(filename, "a", encoding="utf-8")

    def writeLog(self, filename, message):
        if filename not in self.files:
            raise RuntimeError(f"File not open: {filename}")
        ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.files[filename].write(f"[{ts}] {message}\n")
        self.files[filename].flush()

    def readLogs(self, filename):
        p = Path(filename)
        if not p.exists():
            return []
        return p.read_text(encoding="utf-8").splitlines()

    def closeLogFile(self, filename):
        if filename in self.files:
            self.files[filename].close()
            del self.files[filename]

# ---------------- 실행 예시 ----------------
if __name__ == "__main__":
    manager = LogFileManager()
    manager.openLogFile("error.log")
    manager.openLogFile("debug.log")
    manager.openLogFile("info.log")

    manager.writeLog("error.log", "Database connection failed")
    manager.writeLog("debug.log", "User login attempt")
    manager.writeLog("info.log", "Server started successfully")

    errorLogs = manager.readLogs("error.log")

    # 출력 값 확인
    print("// error.log 파일 내용")
    print("\n".join(manager.readLogs("error.log")))
    print("\n// debug.log 파일 내용")
    print("\n".join(manager.readLogs("debug.log")))
    print("\n// info.log 파일 내용")
    print("\n".join(manager.readLogs("info.log")))

    print("\n// readLogs 반환값")
    print(f"errorLogs[0] = '{errorLogs[0]}'")
