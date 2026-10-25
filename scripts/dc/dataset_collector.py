import os
import time
import argparse
import requests
from PIL import Image

def collect_images(url, label_name, width=96, height=96):
    # Створюємо папку для конкретного класу/жесту
    output_dir = os.path.join("dataset", label_name)
    os.makedirs(output_dir, exist_ok=True)
    
    print(f"\n Починаємо збір для класу: [{label_name.upper()}]")
    print(f"Адреса сервера: {url}")
    
    if label_name == "background":
        print(" ПЕРЕКОНАЙТЕСЯ, ЩО В КАДРІ НЕМАЄ РУКИ ТА ІНШИХ ОБ'ЄКТІВ!")
    else:
        print(" Приготуйте руку перед камерою.")
        
    print("Скрипт почне зйомку за 3 секунди...")
    time.sleep(3)
    
    count = 0
    max_photos = 100  # Кількість фото за один підхід
    
    try:
        while count < max_photos:
            response = requests.get(url, timeout=5)
            
            if response.status_code == 200:
                raw_data = response.content
                
                # Перевіряємо розмір буфера (96 * 96 = 9216 байт)
                if len(raw_data) == width * height:
                    img = Image.frombytes('L', (width, height), raw_data)
                    file_path = os.path.join(output_dir, f"{label_name}_{int(time.time()*1000)}.png")
                    img.save(file_path)
                    
                    count += 1
                    print(f"[{count}/{max_photos}] Збережено: {file_path}")
                else:
                    print(f" Помилка: Отримано некоректний розмір даних ({len(raw_data)} байт)")
            else:
                print(f" Помилка запиту: {response.status_code}")
                
            # Пауза між кадрами, щоб встигати змінювати ракурси
            time.sleep(0.4)
            
    except KeyboardInterrupt:
        print("\n Збір зупинено користувачем.")
        
    print(f" Успішно зібрано {count} зображень у папку '{output_dir}'.")

if __name__ == "__main__":
    # Налаштування парсера аргументів командного рядка
    parser = argparse.ArgumentParser(description="Скрипт для збору датасету 96x96 з ESP32-S3 Cam")
    parser.add_argument("--ip", type=str, default="192.168.1.100", help="IP-адреса вашого Куба (за замовчуванням: 192.168.1.100)")
    parser.add_argument("--port", type=str, default="80", help="Порт сервера камери (за замовчуванням: 80)")
    
    args = parser.parse_args()
    
    # Формуємо цільовий URL
    target_url = f"http://{args.ip}:{args.port}/capture_raw"

    while True:
        print(f"\n=== ЗБІР ДАТАСЕТУ ДЛЯ КУБА [{args.ip}:{args.port}] ===")
        print("1. Зібрати КАМІНЬ (rock)")
        print("2. Зібрати НОЖИЦІ (scissors)")
        print("3. Зібрати ПАПІР (paper)")
        print("4. Зібрати ПОРОЖНІЙ ФОН (background)")
        print("5. Вихід")
        
        choice = input("Оберіть опцію (1-5): ")
        
        if choice == '1':
            collect_images(target_url, "rock")
        elif choice == '2':
            collect_images(target_url, "scissors")
        elif choice == '3':
            collect_images(target_url, "paper")
        elif choice == '4':
            collect_images(target_url, "background")
        elif choice == '5':
            print("Скрипт завершено. Бувай!")
            break
        else:
            print("Некоректний вибір, спробуйте ще раз.")