#!/usr/bin/python3


from zpipe import ZPipe, Zephyrgram


repeated = False


def repeat(zp, zgram):
    global repeated
    print(zgram)
    print('hi')
    if repeated:
        return
    repeated = True
    if zgram.opcode == 'repeat':
        return

    zgram.opcode = 'repeat'
    zp.zwrite(zgram)


def main():
    with ZPipe(['../zpipe'], repeat) as zp:
        zp.subscribe('zpipe-example', 'example')
        for i in range(3):
            text = input('enter a message ({}/3) > '.format(i + 1))
            zp.zwrite(Zephyrgram(None, 'zpipe-example', 'example', None,
                                 'zpipe-example', True,
                                 ['zpipe example {}/3'.format(i + 1), text]))
    return 0

if __name__ == '__main__':
    main()
