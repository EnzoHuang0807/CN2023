import requests
import sys
import re

PORT = 8080

if __name__ == '__main__':
    if len(sys.argv) == 2:
        PORT = sys.argv[1]
    elif len(sys.argv) > 2:
        print("Invalid arguments.")
        exit(1)

    req = requests.get(f'http://localhost:{PORT}/')
    assert(req.status_code == 200)

    with open('assets/index.html') as f:
        s = f.read()

    p_number = re.compile(r'{}'.format(s))
    match = p_number.match(req.content.decode())
    assert(match)
    name = match.groups()[0]
    id = match.groups()[1]
    print(f"{name = }")
    print(f"{id = }")


    req = requests.post(f'http://localhost:{PORT}/')
    assert(req.status_code == 405)

    req = requests.get(f'http://localhost:{PORT}/hehehe')
    assert(req.status_code == 404)

    req = requests.post(f'http://localhost:{PORT}/hehehe')
    assert(req.status_code == 404)

