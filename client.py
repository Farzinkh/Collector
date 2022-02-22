from tqdm import tqdm
import requests
import argparse
import os

def download_links(IP):
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
def load_links(resume):
    file1 = open('links.txt', 'r')
    Lines = file1.readlines()
    if not resume: 
        try:
            Lines.remove('LINKS.TXT\n')
        except:
            raise Exception("\nSD card not mount!!")
        try :
            Lines.remove('FRONT\n')
            Lines.remove('TRASH-~1\n')
        except:
            pass
        if not os.path.exists('DATA'):
            os.mkdir("DATA") 
        else:
            for i in os.listdir("DATA"):
                os.remove("DATA/"+i)
    file1.close()
    return Lines
    
def downloading(Lines,IP):
    try:
        count = 0
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
    return count    
    
def print_size(count):
    # get size
    size=0
    for path, dirs, files in os.walk("DATA"):
        for f in files:
            fp = os.path.join(path, f)
            size += os.path.getsize(fp)    
            
    print("downloaded {} instance {} MB".format(count,size/1048576))        

      
def download_manager(ip):
    download_links(ip)
    try:
        Lines=load_links(False)
    except:
        return False
    count=downloading(Lines,ip)
    if count==len(Lines):
        print("Download completed")
    else :
        while True:
            print("Download crupted retrying")
            Lines=load_links(True)
            count=downloading(Lines,ip)
            if count==len(Lines):
                print("Download completed")
                break
    print_size(count)
    return True
    
def main():
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
        download_links(IP)
    Lines=load_links(resume)    
    count=downloading(Lines,IP)
    print_size(count)
    print('DONE!')  

if __name__=="__main__":
    main()