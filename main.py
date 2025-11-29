from fastapi import FastAPI, Request
import uvicorn
import pandas as pd
import os

app = FastAPI()

# ===== CẤU HÌNH CHO TRẠNG THÁI OVERHEATING =====
SAVE_FOLDER = "data_ElectricalFault"          # Thư mục lưu dữ liệu Overheating
os.makedirs(SAVE_FOLDER, exist_ok=True)

SAMPLES_PER_FILE = 600000     # 5 phút với tần số 2kHz
MAX_FILES = 10                # Thu tối đa 10 file

current_file_index = 1
current_samples = 0
recording = False

def get_output_file():
    return f"{SAVE_FOLDER}/Electrical{current_file_index}.csv"

@app.get("/")
async def home():
    return {"message": "BACKEND ELECTRICAL OK", "save_folder": SAVE_FOLDER}

@app.get("/start")
async def start():
    global recording, current_file_index, current_samples

    recording = True
    current_file_index = 1
    current_samples = 0

    # Tạo file mới + header
    file_path = get_output_file()
    with open(file_path, "w") as f:
        f.write("ax,ay,az,current,voltage,temp,label\n")

    print(f"\n=== BẮT ĐẦU THU DỮ LIỆU  Electrical - FILE 1/{MAX_FILES} ===")
    print(f"→ Lưu tại: {file_path}")

    return {"status": "started", "mode": "Electrical Fault"}

@app.get("/stop")
async def stop():
    global recording
    recording = False
    print("\n=== ĐÃ DỪNG THU DỮ LIỆU Electrical ===")
    return {"status": "stopped"}

@app.post("/sensor_batch")
async def receive_batch(request: Request):
    global current_samples, current_file_index, recording

    if not recording:
        return {"status": "ignored", "reason": "not recording"}

    try:
        data = await request.json()
        batch_size = len(data)

        # Thêm nhãn "overheating"
        df = pd.DataFrame(data)
        df["label"] = "Electrical Fault"

        # Ghi vào file hiện tại
        output_file = get_output_file()
        df.to_csv(output_file, mode="a", header=False, index=False)

        current_samples += batch_size

        print(f"Electrical File {current_file_index}/{MAX_FILES} → {current_samples:,} mẫu", end="\r")

        # Đủ mẫu → chuyển file mới
        if current_samples >= SAMPLES_PER_FILE:
            print(f"\n=== HOÀN TẤT Electrical FILE {current_file_index} ({current_samples:,} mẫu) ===")

            current_file_index += 1
            current_samples = 0

            if current_file_index > MAX_FILES:
                recording = False
                print("\n=== ĐÃ THU XONG 10 FILE Electrical Fault – TỰ ĐỘNG DỪNG ===")
                return {"status": "all_done", "mode": "Elecgrical Fault"}

            # Tạo file mới cho lần thu tiếp theo
            new_file = get_output_file()
            with open(new_file, "w") as f:
                f.write("ax,ay,az,current,voltage,temp,label\n")

            print(f"=== BẮT ĐẦU Electrical Fault FILE {current_file_index}/{MAX_FILES} ===")
            print(f"→ Lưu tại: {new_file}")

        return {
            "status": "ok",
            "file": output_file,
            "current_file_index": current_file_index,
            "samples_in_current_file": current_samples,
            "mode": "Electrical Fault"
        }

    except Exception as e:
        print("\nLỗi khi nhận batch:", e)
        return {"error": str(e)}

if __name__ == "__main__":
    print("=== FASTAPI SERVER – CHẾ ĐỘ Electrical ===")
    uvicorn.run(app, host="0.0.0.0", port=8000)