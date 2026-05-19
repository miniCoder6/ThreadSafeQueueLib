import os
import subprocess
import sys

def run():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    results_dir = os.path.join(script_dir, 'results')
    os.chdir(script_dir)

    if not os.path.exists(results_dir):
        os.makedirs(results_dir)

    if not os.path.exists('build'):
        os.makedirs('build')
    os.chdir('build')
    
    subprocess.run(['cmake', '..', '-DCMAKE_BUILD_TYPE=Release'])
    subprocess.run(['cmake', '--build', '.', '--config', 'Release'])
    
    # Use proper Windows path separators for the executable
    exe_path = os.path.join('Release', 'queue_benchmarks.exe')
    results_json = os.path.join(results_dir, 'results_all.json')
    subprocess.run([exe_path, '--benchmark_format=json', f'--benchmark_out={results_json}'])
    
    os.chdir('..')
    subprocess.run([sys.executable, 'plot.py', results_json])

if __name__ == '__main__':
    run()