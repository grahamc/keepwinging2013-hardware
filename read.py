#!/usr/bin/env python

import serial, time, random, urllib, os, sys


def enumerate_serial_ports():
    import _winreg as winreg, itertools
    values = []

    path = 'HARDWARE\\DEVICEMAP\\SERIALCOMM'
    try:
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, path)
    except WindowsError:
        raise IterationError

    for i in itertools.count():
        try:
            val = str(winreg.EnumValue(key, i)[1])
            if val == 'COM1' or val == 'COM2':
                continue
            values.append(val)
        except EnvironmentError:
            break

    return values

def detect_port():
    if os.path.exists('C:\\'):
        paths = enumerate_serial_ports()
    else:
        import glob
        paths = glob.glob('/dev/tty.usbmodem*')

    return paths

ports = detect_port()
ser = None
for port in ports:
    print 'Attempting port ' + port
    try:
        ser = serial.Serial(
            port,
            9600,
            serial.EIGHTBITS,
            serial.PARITY_NONE,
            serial.STOPBITS_ONE,
            3
        )
    except SerialException:
        continue

if ser is None:
    print 'Could not find a KeepWinging reader among the ' + str(len(ports)) + ' ports.'
    sys.exit(1)

lastPing = time.time()
readerVersion = 0
clientVersion = '%GITVER%'

def record_swipe(card):
    burl = 'http://chaincalapp.com:8080/%s';

    params = {
            'rfid': card
            }

    if (sys.argv[1] == 'beer'):
        url = burl % 'eat'
        params['food'] = 'beer'
    elif (sys.argv[1] == 'wing'):
        url = burl % 'eat'
        params['food'] = 'wing'
    elif (sys.argv[1] == 'brownies'):
        url = burl % 'eat'
        params['food'] = 'brownies'
    elif (sys.argv[1] == 'reg'):
        url = burl % 'register/' + card
        os.system('open ' + url);


    params = urllib.urlencode(params)
    f = urllib.urlopen(url, params)
    print f.read()
    return random.randint(0, 100)

def debug(msg):
    if 1:
        print 'Debug: ' + msg

while 1:
    read = ser.readline().strip()
    now = time.time()
    pingDiff = now - lastPing

    if pingDiff > 3:
        print 'No ping in ' + str(pingDiff) + ' seconds.'
    if pingDiff > 10:
        print 'Bailing out.'
        break

    if read:
        results = read.partition(':')

        prefix = results[0]

        if prefix == 'WINGIT':
            print 'Got start: ' + results[2]
            version = results[2]
        elif prefix == 'PING':
            debug('Ping received. Diff: ' + str(pingDiff))
            ser.write("PINGED\n");
            lastPing = now
        elif prefix == 'CARD':
            print 'Got a card: ' + results[2]
            record_swipe(results[2])
            ser.write("CRECVD\n")
        elif prefix == 'DBG':
            debug(results[2])
        elif prefix == 'NOTIFY':
            debug(read)
        else:
            print '?: ' + read
ser.close()


