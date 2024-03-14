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

    genFile('../hw2/client.bin', size='1M')

    sendCMD(c, 'put', 'Usage: put [file]\n', timeout=TIMEOUT)
    sendCMD(c, 'put client.bin', 'Command succeeded.\n', timeout=TIMEOUT)
    cmpFile('../hw2/client.bin', 'assets/pseudo-server/files/client.bin')
    sendCMD(c, 'put notexist', 'Command failed.\n', timeout=TIMEOUT)

    sendCMD(c, 'putv', 'Usage: putv [file]\n', timeout=TIMEOUT)

    sendCMD(c, 'get', 'Usage: get [file]\n', timeout=TIMEOUT)
    sendCMD(c, 'get server.bin', 'Command succeeded.\n', timeout=TIMEOUT)
    cmpFile('../hw2/files/server.bin', 'assets/pseudo-server/files/server.bin')
    sendCMD(c, 'get notexist', 'Command failed.\n', timeout=TIMEOUT)

    sendCMD(c, 'fake command', 'Command not found.\n', timeout=TIMEOUT, expect_aliases=['Command Not Found.\n'])
    sendCMD(c, 'fakecommand', 'Command not found.\n', timeout=TIMEOUT, expect_aliases=['Command Not Found.\n'])

    sendCMD(c, 'quit', 'Bye.\n', timeout=TIMEOUT)

    try:
        sendCMD(c, 'quit', 'Bye.\n', timeout=TIMEOUT)
        exit(1)
    except EOFError:
        print('Client close.')

    delFile('../hw2/client.bin')
    delPath('../hw2/files')