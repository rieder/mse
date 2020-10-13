"""
Run a system from the command line
Adrian Hamers, September 2020
"""

import numpy as np
import numpy.random as randomf
import argparse

from mse import MSE,Particle,Tools


try:
    from matplotlib import pyplot
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False


def add_bool_arg(parser, name, default=False,help=None):
    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument('--' + name, dest=name, action='store_true',help="Enable %s"%help)
    group.add_argument('--no-' + name, dest=name, action='store_false',help="Disable %s"%help)
    parser.set_defaults(**{name:default})

def parse_arguments():

    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("--configuration",                            type=str,                   dest="configuration",               default="fully_nested",             help="Configuration: `fully_nested' or `2+2_quadruple'.")
    parser.add_argument("--masses", "--ms",                           type=float,  nargs="+",     dest="masses",                      default=[40.0,10.0,2.0],            help="Masses/MSun")
    parser.add_argument("--object_types", "--ot",                      type=int,    nargs="+",      dest="object_types",                      default=[1,1,1],            help="Object type (integer): 1: star, 2: planet")
    parser.add_argument("--metallicities", "--zs",                    type=float,  nargs="+",     dest="metallicities",                      default=[0.02,0.02,0.02],           help="Metallicities")
    parser.add_argument("--smas", "--semimajor_axes",                 type=float,  nargs="+",     dest="semimajor_axes",                      default=[15.0,120.0],           help="Semimajor axes (au)")
    parser.add_argument("--es",  "--eccentricities",                  type=float,  nargs="+",     dest="eccentricities",                      default=[0.1,0.1],           help="Eccentricities")
    parser.add_argument("--is", "--inclinations",                     type=float,  nargs="+",     dest="inclinations",                      default=[0.0001,85.0*np.pi/180.0],           help="Inclinations (rad)")
    parser.add_argument("--LANs", "--longitudes_of_ascending_node",   type=float,  nargs="+",     dest="longitudes_of_ascending_node",                      default=[0.01,0.01],           help="Longitudes of the ascending node (rad)")
    parser.add_argument("--APs", "--arguments_of_pericentre",         type=float,  nargs="+",     dest="arguments_of_pericentre",                      default=[0.01,0.01],           help="Arguments of periapsis (rad)")
    
    parser.add_argument("--tend",                           type=float,  dest="end_time",                    default=2.0e7,           help="Integration time (yr)")
    parser.add_argument("--Nsteps",                         type=int,   dest="N_steps",                      default=2000,           help="Number of plot output steps")
    
    parser.add_argument("--plot_filename",                  type=str,  dest="plot_filename",                      default="test1",           help="Plot filename")
    
    ### boolean arguments ###
    add_bool_arg(parser, 'verbose',                         default=False,         help="verbose terminal output")
    add_bool_arg(parser, 'plot',                            default=False,         help="make plots")
    add_bool_arg(parser, 'fancy_plots',                     default=False,         help="make plots")
    
    args = parser.parse_args()

    return args
    
  
if __name__ == '__main__':
    args = parse_arguments()
    
    N_bodies = len(args.masses)
    print("Configuration: ",args.configuration)
    print("N_bodies: ",N_bodies)
    print("Object types:",args.object_types)
    print("Masses/MSun: ",args.masses)
    print("Metallicities: ",args.metallicities)
    print("Semimajor axes (au): ",args.semimajor_axes)
    print("Eccentricities: ",args.eccentricities)
    print("Inclinations (rad): ",args.inclinations)
    print("Longitudes of the ascending node (rad): ",args.longitudes_of_ascending_node)
    print("Arguments of periapsis (rad): ",args.inclinations)
    print("Integration time (yr): ",args.end_time)
    print("Number of plot output steps: ",args.N_steps)
    
    stellar_types = [1 for x in range(N_bodies)]
    Tools.evolve_system(args.configuration,N_bodies,args.masses,args.metallicities,args.semimajor_axes,args.eccentricities,args.inclinations,args.arguments_of_pericentre,args.longitudes_of_ascending_node,args.end_time,args.N_steps,stellar_types=stellar_types,plot_filename=args.plot_filename,object_types=args.object_types,fancy_plots=args.fancy_plots)
