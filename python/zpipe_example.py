#!/usr/bin/python3


from zpipe import ZPipe, Zephyrgram


def main():
    zp = ZPipe(['../zpipe'], lambda p, z: print(z))
    zp.subscribe('zpipe-example', 'example')
    for i in range(3):
        text = input('enter a message ({}/3) > '.format(i + 1))
        zp.zwrite(Zephyrgram(None, 'zpipe-example', 'example', None,
                             'zpipe-example', True,
                             ['zpipe example {}/3'.format(i + 1), text]))
    zp.close_zephyr()
    return 0

if __name__ == '__main__':
    main()
