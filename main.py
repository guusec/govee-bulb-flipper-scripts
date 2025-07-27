import uasyncio as asyncio
import aioble
import bluetooth
from machine import UART
from machine import Pin
import time



led = machine.Pin("LED", machine.Pin.OUT)
for _ in range(3):
    led.on()
    time.sleep(0.2)
    led.off()
    time.sleep(0.2)

name_patterns = [b"ihoment_H6008", b"Govee_H6001"]
TARGET_SERVICE_UUID = bluetooth.UUID('00010203-0405-0607-0809-0a0b0c0d1910')
TARGET_CHAR_UUID = bluetooth.UUID('00010203-0405-0607-0809-0a0b0c0d2b11')

payload_white = bytes.fromhex("33050dffffff00000000000000000000000000c4")
payload_pink = bytes.fromhex("33050dff15d80000000000000000000000000009")

def name_matches(name):
    return any(p in name for p in name_patterns)

async def ble_scan_and_control(color_payload, uart):
    written_devices = set()
    consecutive_no_new = 0
    while consecutive_no_new < 1:
        uart.write(b"Scanning for BLE devices...\r\n")
        found_new = False
        async with aioble.scan(5000, interval_us=30000, window_us=30000, active=True) as scanner:
            async for adv in scanner:
                name = adv.name()
                if not name or not name_matches(name.encode()):
                    continue
                if name in written_devices:
                    uart.write(f"Already handled {name}, skipping.\r\n".encode())
                    continue
                uart.write(f"Handling new device {name}\r\n".encode())
                try:
                    conn = await adv.device.connect()
                except Exception as e:
                    uart.write(f"Connect error: {e}\r\n".encode())
                    continue
                uart.write(b"Connected.\r\n")
                async with conn:
                    try:
                        service = await conn.service(TARGET_SERVICE_UUID)
                        char = await service.characteristic(TARGET_CHAR_UUID)
                        uart.write(b"Writing...\r\n")
                        for i in range(3):
                            await char.write(color_payload)
                            await asyncio.sleep(0.2)
                        uart.write(b"Write(s) done\r\n")
                        written_devices.add(name)
                        found_new = True
                    except Exception as e:
                        uart.write(f"Service/discovery/write failed: {e}\r\n".encode())
                uart.write(b"Disconnected.\r\n")
        if found_new:
            consecutive_no_new = 0
        else:
            consecutive_no_new += 1
            uart.write(f"No new device found. Consecutive unsuccessful scans: {consecutive_no_new}/1\r\n".encode())
        await asyncio.sleep(2)
    uart.write(b"No new devices found after 1 consecutive scans. Stopping.\r\n")

async def uart_cmd_loop():
    uart = UART(0, baudrate=115200, tx=0, rx=1)
    uart.write(b"READY. Send WHITE or PINK on UART to begin. Script stops after 1 scans with no new devices.\r\n")
    buf = b""
    while True:
        while uart.any():
            c = uart.read(1)
            if c in (b"\r", b"\n"):
                cmd = buf.strip().upper()
                buf = b""
                if cmd == b"WHITE":
                    await ble_scan_and_control(payload_white, uart)
                    uart.write(b"READY for next command.\r\n")
                elif cmd == b"PINK":
                    await ble_scan_and_control(payload_pink, uart)
                    uart.write(b"READY for next command.\r\n")
                elif cmd:
                    uart.write(b"ERR\r\n")
            else:
                buf += c
        await asyncio.sleep(0.05)

asyncio.run(uart_cmd_loop())
