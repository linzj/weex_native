import embedjs, sys

def main():
    with open(sys.argv[2], 'w') as f:
        func = getattr(embedjs,sys.argv[1])
        func(f, sys.argv[3], *sys.argv[4:])

if __name__ == '__main__':
    main()
