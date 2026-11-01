from pathlib import Path
import re,json

def mask(s):
    p=re.compile(r'R"([^ ()\\\t\r\n]{0,16})\(.*?\)\1"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/',re.S)
    return p.sub(lambda m: ''.join('\n' if c=='\n' else ' ' for c in m[0]),s)

def units(s):
    m=mask(s); depth=0; par=0; start=0; i=0; out=[]
    while i<len(m):
        c=m[i]
        if c=='#' and depth==0 and par==0:
            j=m.find('\n',i)
            out.append((start,j+1,s[start:j+1],'pre')); start=j+1;i=j+1;continue
        if c=='(':par+=1
        elif c==')':par-=1
        elif c=='{':depth+=1
        elif c=='}':
            depth-=1
            if depth==0 and par==0:
                text=s[start:i+1]; lead=mask(text).strip(); brace=lead.find('{'); pre=lead[:brace]
                if ')' in pre and not re.match(r'(struct|class|enum)\b',pre) and not re.search(r'\b(?:g_|t_)\w+\s*(?:=|\{)',pre):
                    out.append((start,i+1,text,'func')); start=i+1
        elif c==';' and depth==0 and par==0:
            out.append((start,i+1,s[start:i+1],'decl')); start=i+1
        i+=1
    if s[start:].strip():out.append((start,len(s),s[start:],'tail'))
    return out

