from cmath import inf
import numpy as np
import gzip
import glassviewer.catom as pca
from ase import Atom, Atoms
import gzip
import io
import os
from ase.io import write, read
import glassviewer.formats.ase as ptase
import warnings

 

def read_snap(infile, compressed = False):
    """
    Function to read a POSCAR format.

    Parameters
    ----------
    infile : string
        name of the input file

    compressed : bool, optional
        force to read a `gz` zipped file. If the filename ends with `.gz`, use of this keyword is not
        necessary, Default False

    Returns
    -------
    atoms : list of `Atom` objects
        list of all atoms as created by user input

    box : list of list of floats
        list of the type `[[xlow, xhigh], [ylow, yhigh], [zlow, zhigh]]` where each of them are the lower
        and upper limits of the simulation box in x, y and z directions respectively.

    Examples
    --------
    >>> atoms, box = read_poscar('POSCAR')
    >>> atoms, box = read_poscar('POSCAR.gz')
    >>> atoms, box = read_poscar('POSCAR.dat', compressed=True)

    """
    aseobj = read(infile, format="vasp",parallel=False)
    atoms, box = ptase.read_snap(aseobj)
    return atoms, box


def write_snap(sys, outfile, comments="glassviewer", species=None):
    """
    Function to read a POSCAR format.

    Parameters
    ----------
    outfile : string
        name of the input file


    """
    if species is None:
        warnings.warn("Using legacy poscar writer, to use ASE backend specify species")
        write_poscar(sys, outfile, comments=comments)
    else:
        aseobj = ptase.convert_snap(sys, species=species)
        write(outfile, aseobj, format="vasp")


def split_snaps(infile, compressed = False,makedir=True):
    """
    Read in a XDATCAR  trajectory file and convert it to individual time slices.

    Parameters
    ----------

    filename : string
        name of input file

    compressed : bool, optional
        force to read a `gz` zipped file. If the filename ends with `.gz`, use of this keyword is not
        necessary, Default False.

    Returns
    -------
    snaps : list of strings
        a list of filenames which contain individual frames from the main trajectory.

    """
    if makedir:
        newdir='./'+infile+'_dir'
        if not os.path.exists(newdir):
            os.makedirs(newdir,exist_ok=True)
        elif not os.path.exists(newdir+'/'):
            raise ValueError('file with name '+infile+'_dir exists, folder with the same name cannot be created')
    else:
        newdir=''
    raw = infile.split('.')
    if raw[-1] == 'gz' or  compressed:
        f = gzip.open(infile,'rt')
    else:
        f = open(infile,'r')


    #pre-read to find the number of atoms
    for count, line in enumerate(f):
            if count == 6:
                natoms = np.sum([int(i) for i in line.strip().split()])
                break
    f.close()

    #now restart f()
    if raw[-1] == 'gz' or  compressed:
        f = gzip.open(infile,'rt')
    else:
        f = open(infile,'r')



    startblock = 0
    count=1
    header=[]
    nheader=8
    snaps = []
    nohead=False

    for line in f:
        if(startblock==0):
            if(count==1):
                ff = newdir+'/'+".".join([infile, 'snap', str(startblock), 'dat'])
                lines = []
                lines.append(line)
                header.append(line)
                nblock = natoms+nheader
            elif(count<=(nheader-1)):
                lines.append(line)
                header.append(line)
            elif(count<nblock):
                lines.append(line)
            else:
                lines.append(line)
                snaps.append(ff)
                fout = open(ff,'w')
                for wline in lines:
                    fout.write(wline)
                fout.close()
                count=0
                startblock+=1
        else:
            if(count==1):
                ff = newdir+'/'+".".join([infile, 'snap', str(startblock), 'dat'])
                lines = []
                lines.append(line)
                if(line.split()[0]=='Direct' and line.split()[1]=="configuration="):
                    nohead=True
                    nblock = natoms+1
                else:
                    nohead=False
                    nblock = natoms+nheader
            elif(count<nblock):
                lines.append(line)
            else:
                lines.append(line)
                snaps.append(ff)
                fout = open(ff,'w')
                if(nohead==True):
                    for wline in header:
                        fout.write(wline)
                for wline in lines:
                    fout.write(wline)
                fout.close()
                count=0
                startblock+=1
        count+=1

    f.close()

    return snaps

def convert_snap(**kwargs):
    raise NotImplementedError("convert method for mdtraj is not implemented")

def write_poscar(sys, outfile, comments="glassviewer"):
    """
    Function to read a POSCAR format.
    Parameters
    ----------
    outfile : string
        name of the input file
    """

    fout = open(outfile, 'w')

    fout.write(comments+"\n")
    fout.write("   1.00000000000000\n")

    #write box
    vecs = sys.box
    fout.write("      %1.14f %1.14f %1.14f\n"%(vecs[0][0], vecs[0][1], vecs[0][2]))
    fout.write("      %1.14f %1.14f %1.14f\n"%(vecs[1][0], vecs[1][1], vecs[1][2]))
    fout.write("      %1.14f %1.14f %1.14f\n"%(vecs[2][0], vecs[2][1], vecs[2][2]))

    atoms = sys.atoms
    atypes = [atom.type for atom in atoms]
    
    tt, cc  = np.unique(atypes, return_counts=True)
    
    atomgroups = [[] for x in range(len(tt))]
    
    for count, t in enumerate(tt):
        for atom in atoms:
            if int(atom.type) == t:
                atomgroups[count].append(atom)

    fout.write("  ")
    for c in cc:
        fout.write("%d   "%int(c))
    fout.write("\n")

    fout.write("Cartesian\n")

    for i in range(len(atomgroups)):
        for atom in atomgroups[i]:
            pos = atom.pos
            fout.write(" %1.14f %1.14f %1.14f\n"%(pos[0], pos[1], pos[2]))

    fout.close()
