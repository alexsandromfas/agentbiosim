"""
Simple drawing tool using plain pygame.

Controls:
- Hold left mouse button and move to draw.
- Mouse wheel or keys +/- to change brush size.
- C to clear the screen.
- S to save the current image to ./tests/drawing.png
- Esc or window close to exit.

Run: python tests/draw_pygame.py

This file is standalone and has minimal dependencies (pygame).
"""
import os
import pygame
import datetime


def main():
    pygame.init()
    WIDTH, HEIGHT = 1024, 768
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Draw - Pygame simple drawing tool")

    clock = pygame.time.Clock()
    font = pygame.font.SysFont(None, 20)

    # Drawing state
    drawing = False
    last_pos = None
    brush_size = 8
    color = (0, 0, 0)
    bg_color = (255, 255, 255)

    # Create a surface to keep the drawing (so clearing and redrawing is easy)
    canvas = pygame.Surface((WIDTH, HEIGHT))
    canvas.fill(bg_color)

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key in (pygame.K_c, pygame.K_C):
                    # clear
                    canvas.fill(bg_color)
                elif event.key in (pygame.K_s, pygame.K_S):
                    # save
                    try:
                        out_dir = os.path.join(os.path.dirname(__file__), "..")
                        out_dir = os.path.abspath(out_dir)
                        os.makedirs(out_dir, exist_ok=True)
                        filename = os.path.join(out_dir, "drawing_{}.png".format(datetime.datetime.now().strftime("%Y%m%d_%H%M%S")))
                        pygame.image.save(canvas, filename)
                        print(f"Saved drawing to {filename}")
                    except Exception as e:
                        print("Failed to save:", e)
                elif event.key in (pygame.K_MINUS, pygame.K_KP_MINUS):
                    brush_size = max(1, brush_size - 1)
                elif event.key in (pygame.K_EQUALS, pygame.K_PLUS, pygame.K_KP_PLUS):
                    brush_size = min(200, brush_size + 1)

            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:  # left click starts drawing
                    drawing = True
                    last_pos = event.pos
                    pygame.draw.circle(canvas, color, event.pos, brush_size)
                elif event.button == 4:  # wheel up
                    brush_size = min(200, brush_size + 1)
                elif event.button == 5:  # wheel down
                    brush_size = max(1, brush_size - 1)

            elif event.type == pygame.MOUSEBUTTONUP:
                if event.button == 1:
                    drawing = False
                    last_pos = None

            elif event.type == pygame.MOUSEMOTION:
                if drawing and last_pos is not None:
                    current_pos = event.pos
                    # draw line between last and current to avoid gaps
                    pygame.draw.line(canvas, color, last_pos, current_pos, max(1, brush_size * 2))
                    # draw circle at current to make stroke round
                    pygame.draw.circle(canvas, color, current_pos, brush_size)
                    last_pos = current_pos

        # draw canvas to screen
        screen.fill((200, 200, 200))
        screen.blit(canvas, (0, 0))

        # UI overlay: brush size and instructions
        info = f"Brush: {brush_size}  |  +/- or wheel to change  |  C clear  |  S save  |  Esc quit"
        text_surf = font.render(info, True, (10, 10, 10))
        screen.blit(text_surf, (8, HEIGHT - 28))

        # mini preview of current brush
        pygame.draw.rect(screen, (240, 240, 240), (WIDTH - 110, HEIGHT - 52, 96, 44))
        pygame.draw.circle(screen, color, (WIDTH - 60, HEIGHT - 30), brush_size)
        preview_txt = font.render(str(brush_size), True, (10, 10, 10))
        screen.blit(preview_txt, (WIDTH - 80, HEIGHT - 18))

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()


if __name__ == '__main__':
    main()
