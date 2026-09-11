import random

# Konfiguration für CLASS 'M' (4096 Keys, Max Value 512)
# Falls du CLASS 'S' (65536) testen willst, passe dies einfach an!
NUM_KEYS = 8388608
MAX_KEY = 512

filename = "random_array.txt"

with open(filename, "w") as f:
    f.write("/* Ersetze in main.c die alte Deklaration von key_array hiermit: */\n")
    f.write("INT_TYPE key_array[SIZE_OF_BUFFERS] = {\n    ")
    
    for i in range(NUM_KEYS):
        # NAS IS Benchmark Logik: 4 random floats summieren, um eine
        # Gauss-Verteilung zu simulieren (wie im C-Code)
        x = sum(random.random() for _ in range(4))
        val = int((MAX_KEY / 4) * x)
        
        f.write(f"{val}")
        
        if i < NUM_KEYS - 1:
            f.write(", ")
        
        # Zeilenumbruch alle 15 Zahlen für bessere Lesbarkeit in C
        if (i + 1) % 15 == 0:
            f.write("\n    ")
            
    f.write("\n};\n")

print(f"Erfolgreich! {NUM_KEYS} Werte wurden in '{filename}' geschrieben.")
print("Du kannst die Datei nun oeffnen, Command + A druecken und sie kopieren.")