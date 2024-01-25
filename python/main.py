import pygame, sys
from grid import Grid
from blocks import *

pygame.init()
dark_blue = (44, 44, 127)

screen = pygame.display.set_mode((300, 600))  # width, height
pygame.display.set_caption("Python Tetris")

clock = pygame.time.Clock() # frame control

game_grid = Grid()

block = TBlock()

while True:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            sys.exit()
    #Drawing
    screen.fill(dark_blue)
    game_grid.draw(screen)
    block.draw(screen)
    
    pygame.display.update()
    clock.tick(60)

