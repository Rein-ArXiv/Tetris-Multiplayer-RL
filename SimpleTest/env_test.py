import simple_env
import random

env = simple_env.SimpleEnv()

obs = env.reset()

input("Press Enter to Start")

for i in range(100):
    action = random.randint(0, 10)
    obs, reward, done = env.step(action)
    
    print(f"Choice: {action}\t Answer: {obs}")
    if (done):
        print("Clear!")
        break

