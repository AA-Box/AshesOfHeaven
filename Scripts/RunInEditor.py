"""Run a python file inside the RUNNING Unreal editor, instead of in a fresh commandlet.

    python3 Scripts/RunInEditor.py Scripts/CompleteSoulCity.py

Needed because a `-nullrhi` commandlet has no ticked physics scene: `line_trace_single`
reports no blocking hit anywhere, so any script that has to find the ground under a point
silently places everything at the wrong height. The running editor traces correctly.

Requires PythonScriptPlugin remote execution enabled in Config/DefaultEngine.ini, which this
project has. Multicast does not work on this machine - a unicast datagram to 127.0.0.1:6766
with no `dest` field passes the editor's receive filter, and it then connects back over TCP.
"""
import json, socket, sys, uuid

NODE = str(uuid.uuid4())
UDP_PORT = 6766
TCP_PORT = 6776


def message(kind, data=None):
    m = {"version": 1, "magic": "ue_py", "source": NODE, "type": kind}
    if data is not None:
        m["data"] = data
    return json.dumps(m).encode("utf-8")


def run(code, mode="ExecuteFile", timeout=1800.0):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", TCP_PORT))
    listener.listen(1)
    listener.settimeout(30.0)

    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.sendto(message("open_connection",
                       {"command_ip": "127.0.0.1", "command_port": TCP_PORT}),
               ("127.0.0.1", UDP_PORT))
    conn, _ = listener.accept()
    conn.settimeout(timeout)
    conn.sendall(message("command", {"command": code, "unattended": True, "exec_mode": mode}))

    chunks = b""
    while True:
        part = conn.recv(65536)
        if not part:
            break
        chunks += part
        try:
            reply = json.loads(chunks.decode("utf-8"))
            break
        except Exception:
            continue
    conn.close()
    listener.close()
    udp.sendto(message("close_connection"), ("127.0.0.1", UDP_PORT))
    return reply


if __name__ == "__main__":
    source = open(sys.argv[1]).read() if len(sys.argv) > 1 else "unreal.log('hello')"
    result = run(source)
    data = result.get("data", {})
    print("success:", data.get("success"))
    for line in data.get("output", []):
        print(line.get("type", ""), line.get("output", "").rstrip())
    if data.get("result"):
        print("result:", data["result"])
