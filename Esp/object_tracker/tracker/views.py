from django.conf import settings  # settings modülünü dahil edin
from django.apps import AppConfig  # AppConfig sınıfını içe aktarın
import cv2
import requests
from django.shortcuts import render
from django.http import JsonResponse
import threading
import time
import numpy as np
import os

# ESP32-CAM bağlantı ayarları
ESP32_STREAM_URL = "http://192.168.137.7:80/stream"
ESP32_MOVE_URL = "http://192.168.137.7:80/move"

# Yüz algılama model dosyalarının yolları
MODEL_DIR = os.path.join(settings.BASE_DIR, 'tracker', 'models')
PROTOTXT_PATH = os.path.join(MODEL_DIR, 'deploy.prototxt')
MODEL_PATH = os.path.join(MODEL_DIR, 'res10_300x300_ssd_iter_140000.caffemodel')

# DNN modeli yükleme
net = cv2.dnn.readNetFromCaffe(PROTOTXT_PATH, MODEL_PATH)

# Thread kilidi ve yüz bilgileri
lock = threading.Lock()
current_face = None

def index(request):
    
    process_stream()
    return render(request, 'tracker/index.html', {'stream_url': ESP32_STREAM_URL})

def process_stream():
    
    global current_face

    while True:
        try:
           
            response = requests.get(ESP32_STREAM_URL, stream=True, timeout=10)
            if response.status_code != 200:
                print(f"Kamera akışı alınamadı: {response.status_code}")
               
                time.sleep(2)
                continue
                
            bytes_data = bytes()
            frame_count = 0
            start_time = time.time()

            
            for chunk in response.iter_content(chunk_size=1024):
                if not chunk:
                    break

                bytes_data += chunk
                a = bytes_data.find(b'\xff\xd8')  
                b = bytes_data.find(b'\xff\xd9')  
                if a != -1 and b != -1 and b > a:
                    jpg = bytes_data[a:b+2]
                    bytes_data = bytes_data[b+2:]
                    img = cv2.imdecode(np.frombuffer(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)
                    if img is not None:
                        detect_and_move(img)
                        frame_count += 1

                
                if frame_count >= 10 or (time.time() - start_time) > 3.0:
                    break

        except Exception as e:
            print(f"Stream işleme hatası: {e}")

        finally:
           
            try:
                response.close()
            except:
                pass

        
       

def detect_and_move(img):
    """
    Bir frame üzerinde yüz tespiti yapar ve servo komutu gönderir.
    """
    global current_face, lock

    (h, w) = img.shape[:2]

    
    blob = cv2.dnn.blobFromImage(cv2.resize(img, (300, 300)), 1.0, (300, 300), (104.0, 177.0, 123.0))
    net.setInput(blob)
    detections = net.forward()
    if detections.shape[2] > 0:
        i = np.argmax(detections[0, 0, :, 2])
        confidence = detections[0, 0, i, 2]

       
        CONFIDENCE_THRESHOLD = 0.5
        if confidence > CONFIDENCE_THRESHOLD:
           
            box = detections[0, 0, i, 3:7] * np.array([w, h, w, h])
            (startX, startY, endX, endY) = box.astype("int")

            face_center_x = (startX + endX) // 2
            face_center_y = (startY + endY) // 2

            
            camera_center_x = w // 2
            camera_center_y = h // 2

           
            dx = ((face_center_x - camera_center_x) / camera_center_x) * 100
            dy = ((face_center_y - camera_center_y) / camera_center_y) * 100

            print(f"Yüz merkezi: ({face_center_x}, {face_center_y}), Uzaklık: dx={dx:.2f}, dy={dy:.2f}")

            with lock:
                current_face = {'dx': dx, 'dy': dy}

        
        
            send_move_command(dx, dy)
        else:
            with lock:
                current_face = None
    else:
        with lock:
            current_face = None

def send_move_command(dx, dy):
    """
    Servo motorları hareket ettirmek için ESP32'ye komut gönderir.
    """
    MAX_DELTA = 10  # Maksimum delta hareketi
    dx = -max(-MAX_DELTA, min(MAX_DELTA, dx))
    dy = max(-MAX_DELTA, min(MAX_DELTA, dy))

    payload = {'dx': dx, 'dy': dy}
    try:
        response = requests.post(ESP32_MOVE_URL, data=payload, timeout=5)
        if response.status_code == 200:
            print(f"Servo hareketi gönderildi: {response.text}")
        else:
            print(f"Servo hareketi başarısız: {response.status_code}")
    except Exception as e:
        print(f"Servo komut gönderme hatası: {e}")

class TrackerConfig(AppConfig):
    """
    Uygulama yapılandırması, yüz algılama işlemini başlatır.
    """
    name = 'tracker'

    def ready(self):
        print("TrackerConfig ready: Arka planda yüz algılama başlatılıyor.")
        threading.Thread(target=process_stream, daemon=True).start()

def detect_face(request):
    """
    Yüz algılama endpointi. Şu anda dummy bir yanıt döndürmektedir.
    """
    return JsonResponse({"message": "Face detection endpoint çalışıyor!"})

def status_view(request): 
    
    return JsonResponse({"status": "OK", "message": "Tracker is running"})
