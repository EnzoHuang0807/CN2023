from pwn import * 
from assets.utils import *

TIMEOUT = 2

def sendCMD(p, cmd, expect, timeout=TIMEOUT, expect_aliases=[]):
    recv = p.sendlineafter('> '.encode(), cmd.encode(), timeout)
    print(f'send = {cmd}', end='')
    assert(recv.decode() == '> ')

    recv = p.recvline()
    print(f' recv = {recv}')
    
    if recv.decode() != expect:
        for alias in expect_aliases:
            if recv.decode() == alias:
                return
        assert(0)

if __name__ == '__main__':
    delPath('../hw2/files')

    pty = process.PTY
    c = process('./client 127.0.0.1 2023 demo:123', shell=True, cwd='../hw2', stdin=pty, stdout=pty) 
    # argv=['./hw2/client', '127.0.0.1', '8080', 'username:password']
    # r = process(argv=argv)

    genFile('../hw2/L4RGebUtNoT7o01ArgE', size='100M')
    genFile('../hw2/a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o', size='1M')
    genFile('../hw2/iS7H@TL3g4L?', size='5M')

    sendCMD(c, 'put L4RGebUtNoT7o01ArgE', 'Command succeeded.\n', timeout=10)
    cmpFile('../hw2/L4RGebUtNoT7o01ArgE', 'assets/pseudo-server/files/L4RGebUtNoT7o01ArgE')
    sendCMD(c, 'put a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o', 'Command succeeded.\n', timeout=TIMEOUT)
    cmpFile('../hw2/a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o', 'assets/pseudo-server/files/a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o')
    
    sendCMD(c, 'get L4RGebUtNoT7o01ArgE', 'Command succeeded.\n', timeout=10)
    cmpFile('assets/pseudo-server/files/L4RGebUtNoT7o01ArgE', '../hw2/files/L4RGebUtNoT7o01ArgE')
    sendCMD(c, 'get a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o', 'Command succeeded.\n', timeout=TIMEOUT)
    cmpFile('assets/pseudo-server/files/a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o', '../hw2/files/a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o')

    sendCMD(c, 'put iS7H@TL3g4L?', 'Command succeeded.\n', timeout=TIMEOUT)
    cmpFile('../hw2/iS7H@TL3g4L?', 'assets/pseudo-server/files/iS7H@TL3g4L?')
    sendCMD(c, 'get iS7H@TL3g4L?', 'Command succeeded.\n', timeout=TIMEOUT)
    cmpFile('assets/pseudo-server/files/iS7H@TL3g4L?', '../hw2/files/iS7H@TL3g4L?')

    sendCMD(c, 'quit', 'Bye.\n', timeout=TIMEOUT)

    try:
        sendCMD(c, 'quit', 'Bye.\n', timeout=TIMEOUT)
        exit(1)
    except EOFError:
        print('Client close.')

    delFile('../hw2/L4RGebUtNoT7o01ArgE')
    delFile('../hw2/a 5 a 5 a a 5 5 5 o o 1 a 1 a a 5 5 5 o o')
    delFile('../hw2/iS7H@TL3g4L?')
    delPath('../hw2/files')