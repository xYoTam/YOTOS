import socket
import threading
import argparse
import pygame

PORT = 4444

WIDTH, HEIGHT = 800, 500
FONT_SIZE = 18
BG = (18, 18, 18)
FG = (200, 200, 200)
INPUT_COLOR = (255, 255, 100)

lock = threading.Lock()
shell_prompt = "YOTOS>"

def recv_thread(sock, lines):
    # Receive data from YOTOS (output of commands)
    # Keep receiving until /n, and add it to the lines for printing
    buf = ''
    while True:
        try:
            data = sock.recv(256).decode(errors='replace')
            if data:
                buf += data
                while '\n' in buf:
                    line, buf = buf.split('\n', 1)
                    with lock:
                        lines.append(line)
        except socket.timeout:
            pass
        except Exception:
            break


def render(screen, font, lines, current_input):
    screen.fill(BG)
    w, h = screen.get_size()
    line_h = FONT_SIZE + 4
    max_lines = (h - line_h - 10) // line_h

    with lock:
        visible = lines[-max_lines:] if len(lines) > max_lines else lines[:]  # get only the lines fitting on screen

    for i, line in enumerate(visible):  # draw lines on screen
        surf = font.render(line, True, FG)
        screen.blit(surf, (8, 8 + i * line_h))

    # render "> current_input_" in yellow at the very bottom of the screen (the _ acts as a cursor).
    prompt = shell_prompt + current_input + '_'
    surf = font.render(prompt, True, INPUT_COLOR)
    screen.blit(surf, (8, h - line_h - 6))
    pygame.display.flip()


def main_loop(screen, font, lines, current_input, clock, sock):
    running = True
    # Main loop - check for events (different keys pressed)
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_RETURN:
                    cmd = current_input
                    with lock:
                        lines.append(shell_prompt + cmd)
                    sock.sendall((cmd + '\n').encode())
                    current_input = ''

                elif event.key == pygame.K_BACKSPACE:
                    current_input = current_input[:-1]

                elif event.key == pygame.K_ESCAPE:
                    running = False

                else:
                    char = event.unicode
                    if char and char.isprintable():
                        current_input += char

        render(screen, font, lines, current_input)
        clock.tick(60)


def main(args):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((args.host, PORT))
    sock.settimeout(0.05)

    lines = [f'Connected to UART at {args.host}:{PORT}. Type and press Enter to send.', '']
    current_input = ''

    threading.Thread(target=recv_thread, args=(sock, lines), daemon=True).start()   # Thread for receiving messages from YOTOS

    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.RESIZABLE)
    pygame.display.set_caption('YOTOS UART')
    font = pygame.font.SysFont('Courier New', FONT_SIZE)
    clock = pygame.time.Clock()

    main_loop(screen, font, lines, current_input, clock, sock)

    sock.close()
    pygame.quit()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='YOTOS UART client')
    parser.add_argument('host', help='Server IP address')
    args = parser.parse_args()
    main(args)
