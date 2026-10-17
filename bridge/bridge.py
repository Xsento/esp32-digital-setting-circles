import asyncio
import json
import struct
import time
from astropy.coordinates import SkyCoord, EarthLocation, AltAz
from astropy.time import Time
import astropy.units as u

current_az = 0.0
current_alt = 0.0  


# ==========================================

class AstroObject:
    def __init__(self, lat, lon, elevation=0):
        self.location = EarthLocation(lat=lat*u.deg, lon=lon*u.deg, height=elevation*u.m)

    def altaz_to_radec(self, azimuth, altitude):
        obstime = Time.now()
        altaz_frame = AltAz(obstime=obstime, location=self.location)
        altaz_coord = SkyCoord(az=azimuth*u.deg, alt=altitude*u.deg, frame=altaz_frame)
        radec_coord = altaz_coord.transform_to('icrs')

        ra_hours = radec_coord.ra.hour
        dec_degrees = radec_coord.dec.deg
        
        return ra_hours, dec_degrees

# ==========================================

async def esp32_client():
    HOST = 'dsc.local'
    PORT = 5000
    
    try:
        reader, writer = await asyncio.open_connection(HOST, PORT)
        print("Connected to ESP32!\n")

        read_task = asyncio.create_task(espRead(reader))
        write_task = asyncio.create_task(espWrite(writer))

        done, pending = await asyncio.wait(
            [read_task, write_task], 
            return_when=asyncio.FIRST_COMPLETED
    )

    except KeyboardInterrupt:
        print("\nConnection closed.")
    except Exception as e:
        print(f"Connection error: {e}")
        print("Retrying in 2 seconds...")
        await asyncio.sleep(2)

    for task in pending:
        task.cancel()


async def espRead(reader):
    global current_az, current_alt
    buffer = "s"

    while True:
        raw = await reader.read(1024)
        
        if not raw:
            print("DSC disconnected.")
            break
            
        data = raw.decode('utf-8')
        buffer += data
        
        while '\n' in buffer:
            index = buffer.find('\n')
            try:
                parsed = json.loads(buffer[0:index])
                if (parsed["type"] == "position"):
                    current_az = parsed["az"]
                    current_alt = parsed["alt"]
                    
                else:
                    print("Message from DSC:", parsed.get("msg", parsed))
                    
            except Exception as ex:
                print(f"Error: {ex}")
                print(buffer)

            buffer = buffer[index+1:]

async def espWrite(writer):
    loop = asyncio.get_running_loop()
    
    while True:
        cmd = await loop.run_in_executor(None, input, "> ")
        
        if cmd.strip():
            message = cmd.strip() + "\n"
            writer.write(message.encode('utf-8'))
            
            # Wymuszamy natychmiastowe wysłanie pakietu przez bufor TCP
            await writer.drain()


# ==========================================

async def stellariumHandler(reader, writer):
    global current_az, current_alt
    print("[Stellarium] Connected to the bridge")
    
    try:
        while True:
            ra_hours, dec_degrees = await asyncio.to_thread(
                telescope.altaz_to_radec, current_az, current_alt
            )
            
            # 2. Przeliczenie na format Stellarium
            ra_int = int((ra_hours / 24.0) * 4294967296) % 4294967296
            dec_int = int((dec_degrees / 90.0) * 1073741824)
            czas_mikrosekundy = int(time.time() * 1000000)

            ramka = struct.pack('<hhQIiI', 24, 0, czas_mikrosekundy, ra_int, dec_int, 0)
            writer.write(ramka)
            await writer.drain()
            
            await asyncio.sleep(0.33)
            
    except Exception as e:
        print(f"[Stellarium] Error: {e}")
    finally:
        print("[Stellarium] Disconnected.")
        writer.close()

# ==========================================

async def main():
    latitude = 21
    longitude = 37
    elevation = 67
    global telescope
    telescope = AstroObject(latitude, longitude, elevation)
    print("Starting the bridge...")
    
    espTask = asyncio.create_task(esp32_client())
    
    stellariumServer = await asyncio.start_server(stellariumHandler, '127.0.0.1', 10001)
    
    async with stellariumServer:
        # await asyncio.gather sprawia, że oba procesy działają jednocześnie
        await asyncio.gather(
            espTask,
            stellariumServer.serve_forever()
        )


if __name__ == "__main__":
    asyncio.run(main())