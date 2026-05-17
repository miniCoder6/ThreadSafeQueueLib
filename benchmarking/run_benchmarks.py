import os
import subprocess
import sys

def run():
    if not os.path.exists('build'):
        os.makedirs('build')
    os.chdir('build')
    subprocess.run(['cmake', '..', '-DCMAKE_BUILD_TYPE=Release'])
    subprocess.run(['cmake', '--build', '.', '--config', 'Release'])
    subprocess.run(['./Release/queue_benchmarks.exe', '--benchmark_format=json', '--benchmark_out=../results_all.json'])
    os.chdir('..')
    subprocess.run([sys.executable, 'plot.py', 'results_all.json'])

if __name__ == '__main__':
    run()