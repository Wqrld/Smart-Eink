from PIL import Image, ImageDraw, ImageFont, ImageEnhance
from datetime import datetime, timedelta
import requests
from flask import Flask, send_file
from io import BytesIO
from buienradar.buienradar import (get_data, parse_data)
from buienradar.constants import (CONTENT, RAINCONTENT)
from urllib.request import urlopen
from functools import lru_cache
import configparser
import json

config = configparser.ConfigParser()
config.read('config.ini')
# flask --app convert run --host=0.0.0.0
# systemctl status imagegen


imageIndex = 0
images = json.loads(config.get('images', 'images'))


# Write image to cpp file
def save_image_data(quanti):
    f = open("imagedata.cpp","w")
    f.write('#include "imagedata.h"\n')
    f.write('#include <avr/pgmspace.h>\n')
    f.write('const unsigned char Image4color[' + str(int((792*272)/4) + 1) + '] PROGMEM = { // keep data in flash\n')
    for y in range(272):
        for x in range(int(792/4)):
            byte = (quanti.getpixel((x*4,y)) << 6) | (quanti.getpixel(((x*4)+1,y)) << 4) | (quanti.getpixel(((x*4)+2,y)) << 2) | quanti.getpixel(((x*4)+3,y))
            f.write(f"{hex(byte)}, ")
    f.write('0x00\n};\n')
    f.close()

app = Flask(__name__)

@app.route("/")
def hello_world():
    return "<p>Hello, World!</p>"

# Parameter is for caching
@lru_cache(maxsize=2)
def getweather(hour):
    try:
        weather_result = get_data(latitude=config.getfloat("weather", "latitude"),
                      longitude=config.getfloat("weather", "longitude"),
                      )
        weather_result = parse_data(weather_result[CONTENT], weather_result[RAINCONTENT], config.getfloat("weather", "latitude"), config.getfloat("weather", "longitude"), timeframe=120)['data']
        return weather_result
    except Exception:
        return None

imagemap = {
    'GREEN': './green_icon.png',
    'PAPER': './paper_icon.png',
    'PACKAGES': './packages_icon.png'
}

# Parameter is for caching
@lru_cache(maxsize=12)
def gettrash(hour):
    url = 'https://wasteapi.ximmio.com/api/GetCalendar'
    myobj = {'companyCode': config.get('waste', 'companyCode'), 'uniqueAddressID': config.get('waste', 'uniqueAddressID'), 'startDate': datetime.now().strftime('%Y-%m-%d'), 'endDate': (datetime.now() + timedelta(days=0)).strftime('%Y-%m-%d')}

    x = requests.post(url, json = myobj)
    datalist = x.json()["dataList"]
    print(datalist)
    if len(datalist) == 0:
        return None, None, None
    next_pickup = min(datalist, key=lambda d: datetime.fromisoformat(d['pickupDates'][0]))
    next_pickup_date = datetime.fromisoformat(next_pickup['pickupDates'][0])
    next_pickup_type = next_pickup['_pickupTypeText']
    return next_pickup_date, next_pickup_type, imagemap[next_pickup_type] if next_pickup_type in imagemap else None



# Load image, draw, and quantize.
def parse_image():
    with Image.open(images[imageIndex]) as im:
        pal_image = Image.new("P", (1,1))
        pal_image.putpalette( (0,0,0,  255,255,255, 255, 255, 0, 255,0,0 ) )
        quanti = im.resize((792, 272))
        enhancer = ImageEnhance.Brightness(quanti)
        # to reduce brightness by 50%, use factor 0.5
        quanti = enhancer.enhance(1.05)
        # enhancer = ImageEnhance.Color(quanti)
        # increase color saturation by 50%, use factor 1.5
        # quanti = enhancer.enhance(2)


        # Change arduino to hourly. Maybe dont refresh between 3 and 6 to save cycles
        # fix deepsleep timer accuracy

        # https://pypi.org/project/buienradar/
        weather_result = getweather(datetime.now().strftime(f'%B %d | %HH')) # Cache based on datetime
        print("Weather data:")
        print(weather_result)

        if weather_result:
            forecast = weather_result.get('forecast') or []
            condition = forecast[0].get('condition', {}) if forecast else {}
            image_url = condition.get('image')
            if image_url:
                overlay = Image.open(BytesIO(urlopen(image_url).read())).resize((60, 60))
                overlayEnhancer = ImageEnhance.Brightness(overlay)
                overlay = overlayEnhancer.enhance(0)
                quanti.paste(overlay, (0, 0), overlay)

        trash_result = gettrash(datetime.now().strftime(f'%B %d | %HH')) # Cache based on datetime
        if trash_result[2] is not None:
            overlay = Image.open(trash_result[2]).resize((45, 45))
            overlayEnhancer = ImageEnhance.Brightness(overlay)
            overlay = overlayEnhancer.enhance(0)
            quanti.paste(overlay, (792-45-5, 5), overlay)

        draw = ImageDraw.Draw(quanti)
        font = ImageFont.truetype("arial.ttf", 45)
        message = datetime.now().strftime(f'%B %d | %HH')

        if weather_result:
            forecast = weather_result.get('forecast') or []
            max_temp = forecast[0].get('maxtemp') if forecast else None
            temperature = weather_result.get('temperature')
            if temperature is not None and max_temp is not None:
                message += f" | {temperature}/{max_temp}*C"
            now = weather_result.get('condition', {}).get('detailed')
            if now:
                message += f" \n{now}"

        _, _, w, h = draw.textbbox((0, 0), message, font=font)
        draw.text(((792-w)/2, 170), message, font=font, fill="#ffffff", stroke_width=2, stroke_fill="#000000")


        quanti = quanti.convert("RGB").quantize(palette=pal_image)
        # quanti.save("fctwente-embleem-792x272-resized.png")
        # save_image_data(quanti)
        return quanti


# Serve raw byte image
@app.route("/raw_image_bytes")
def raw_image_bytes():
    quanti = parse_image()
    ba = bytearray()
    for y in range(272):
        for x in range(792 // 4):
            byte = (quanti.getpixel((x*4,y)) << 6) | (quanti.getpixel(((x*4)+1,y)) << 4) | (quanti.getpixel(((x*4)+2,y)) << 2) | quanti.getpixel(((x*4)+3,y)) # type: ignore
            ba.append(byte)
    return send_file(BytesIO(ba), mimetype='application/octet-stream')

# Serve raw half byte image
@app.route("/raw_image_bytes/<half>") # type: ignore
def raw_image_bytes_h(half):
    quanti = parse_image()
    ba = bytearray()

    if half == "0":
        global imageIndex
        imageIndex = imageIndex + 1 if imageIndex < len(images) - 1 else 0

    x_start = 0 if half == "0" else 396
    x_end = x_start + 396

    quanti.save("quanti.png")
    quanti = quanti.crop((x_start, 0, x_end, 272))
    quanti.save(f"half_{half}.png")

    for y in range(272):
        for x in range(396 // 4):
            byte = (quanti.getpixel((x*4,y)) << 6) | (quanti.getpixel(((x*4)+1,y)) << 4) | (quanti.getpixel(((x*4)+2,y)) << 2) | quanti.getpixel(((x*4)+3,y)) # type: ignore
            ba.append(byte)
            # TODO: has a 2/3 correction factor because of bad timing. Needs to be fixed *somewhere* in the arduino code.
    return send_file(BytesIO(ba), mimetype='application/octet-stream'), {'time_until_next_hour_ms': int((2/3) * (3600000 - datetime.now().minute * 60000 - datetime.now().second * 1000))}
            
# Serve image in user-readable format
@app.route("/image")
def image():
    global imageIndex
    imageIndex = imageIndex + 1 if imageIndex < len(images) - 1 else 0

    quanti = parse_image()
    img_io = BytesIO()
    quanti.save(img_io, 'PNG', quality=70)
    img_io.seek(0)
    return send_file(img_io, mimetype='image/png')
