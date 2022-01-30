from tkinter import EXCEPTION
from tqdm import tqdm
import requests
import os

f = open("links.txt",'wb')
f.write(requests.get('http://192.168.4.1/links').content)
f.close()

file1 = open('links.txt', 'r')
Lines = file1.readlines()
if not os.path.exists('DATA'):
    os.mkdir("DATA") 
count = 0
Lines.remove('TRASH-~1\n')
Lines.remove('FRONT\n')
Lines.remove('LINKS.TXT\n')
try:
    print("downloading...")
    for line in tqdm(Lines):
        line=line.rstrip("\n")
        count += 1
        f = open('DATA/'+line,'wb')
        f.write(requests.get('http://192.168.4.1/{}'.format(line)).content)
        f.close()
except KeyboardInterrupt:
    print("downloading cancelled at {}".format(line))
except Exception as e:
    print("ESP network missed at downloading {}".format(line))
# get size
size=0
for path, dirs, files in os.walk("DATA"):
    for f in files:
        fp = os.path.join(path, f)
        size += os.path.getsize(fp)    
        
print("downloaded {} instance {} MB".format(count,size/1048576))        
print('DONE!')        