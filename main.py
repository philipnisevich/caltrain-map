from fastapi import FastAPI
import requests
from google.transit import gtfs_realtime_pb2
from geopy.distance import geodesic
from stations import STATIONS
import time
import os
from pathlib import Path

# ================== CONFIG ==================
def _load_env_file() -> None:
    """Load key=value pairs from local .env file into process env."""
    env_path = Path(__file__).with_name(".env")
    if not env_path.exists():
        return

    for raw_line in env_path.read_text().splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip())


_load_env_file()
API_KEY = os.getenv("API_KEY")
if not API_KEY:
    raise RuntimeError("Missing API_KEY. Add it to a local .env file.")
GTFS_URL = f"https://api.511.org/transit/vehiclepositions?api_key={API_KEY}&agency=CT"
MAX_STATION_DISTANCE = 500  # meters
# ============================================

app = FastAPI()

def get_direction(trip):
    """Return 'NORTH' or 'SOUTH' for Caltrain correctly"""
    if not trip.HasField("direction_id"):
        return "UNKNOWN"
    # Caltrain convention: 0 = Southbound (SF → Gilroy), 1 = Northbound (Gilroy → SF)
    return "SOUTH" if trip.direction_id == 1 else "NORTH"

def get_closest_station(train_pos):
    """Return the nearest station and distance in meters"""
    closest_station = None
    closest_distance = float("inf")
    for station, coords in STATIONS.items():
        dist = geodesic(train_pos, coords).meters
        if dist < closest_distance:
            closest_station = station
            closest_distance = dist
    return closest_station, closest_distance

def get_train_directions():
    feed = gtfs_realtime_pb2.FeedMessage()
    try:
        response = requests.get(GTFS_URL, timeout=10)
        response.raise_for_status()

        # Handle 511 returning "No Data Found" (JSON) instead of protobuf
        if b"No Data Found" in response.content:
            return []

        feed.ParseFromString(response.content)
    except Exception as e:
        print("Error fetching or parsing GTFS feed:", e)
        return []

    trains = []

    for entity in feed.entity:
        if not entity.HasField("vehicle"):
            continue
        if not entity.vehicle.HasField("position"):
            continue

        lat = entity.vehicle.position.latitude
        lon = entity.vehicle.position.longitude
        if lat is None or lon is None:
            continue

        train_pos = (lat, lon)
        station, distance = get_closest_station(train_pos)
        if distance > MAX_STATION_DISTANCE:
            continue

        direction = get_direction(entity.vehicle.trip)
        trains.append({
            "station": station,
            "direction": direction
        })

    return trains

@app.get("/caltrain")
def caltrain_status():
    trains = get_train_directions()
    return {
        "timestamp": int(time.time()),
        "trains": trains
    }
