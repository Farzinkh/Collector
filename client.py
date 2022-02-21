from tqdm import tqdm
import requests
import argparse
import os

parser=argparse.ArgumentParser(description='script to download from ESP',epilog='Attention : if it is going to resume download add c command')
parser.add_argument("operation",metavar='operation',help="c for resume d for download" )
parser.add_argument('-ip',"--serverip",metavar='serverip',help="Enter ip of server")
args=parser.parse_args()

if args.operation=="c":
    resume=True
    IP=args.serverip   
else:
    resume=False
    IP=args.serverip 
    print('Downloading links!')
    f = open("links.txt",'wb')
    f.write(requests.get('http://{}/links'.format(IP)).content)
    f.close()
    print('Ready')

def remove_lines(line):
    found=False
    with open('links.txt', "r") as f:
        Lines = f.readlines()
    with open("links.txt", "w") as f:
        for l in Lines:
            if found:    
                f.write(l)    
            else:
                if l.strip("\n") == line:
                    found=True    

file1 = open('links.txt', 'r')
Lines = file1.readlines()
if not os.path.exists('DATA'):
    os.mkdir("DATA") 
count = 0
if not resume: 
    Lines.remove('LINKS.TXT\n')
    try :
        Lines.remove('FRONT\n')
        Lines.remove('TRASH-~1\n')
    except:
        pass
file1.close()
try:
    print("downloading...")
    for line in tqdm(Lines):
        line=line.rstrip("\n")
        count += 1
        f = open('DATA/'+line,'wb')
        f.write(requests.get('http://{}/{}'.format(IP,line)).content)
        f.close()
except KeyboardInterrupt:
    print("downloading cancelled at {} run resume by `python client.py c".format(line))
    remove_lines(line)
except Exception as e:
    print("ESP network missed at downloading {} run resume by `python client.py c".format(line))
    remove_lines(line)
# get size
size=0
for path, dirs, files in os.walk("DATA"):
    for f in files:
        fp = os.path.join(path, f)
        size += os.path.getsize(fp)    
        
print("downloaded {} instance {} MB".format(count,size/1048576))        
print('DONE!')        
