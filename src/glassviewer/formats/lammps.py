from multiprocessing.sharedctypes import Value
import numpy as np
import gzip
import glassviewer.catom as pca
from ase import Atom, Atoms
import gzip
import io
import os

#functions that are not wrapped from C++
def read_snap(infile, compressed = False, check_triclinic=False, customkeys=None):
    """
    Function to read a lammps dump file format - single time slice.

    Parameters
    ----------
    infile : string
        name of the input file

    compressed : bool, optional
        force to read a `gz` zipped file. If the filename ends with `.gz`, use of this keyword is not
        necessary. Default True.

    check_triclinic : bool, optional
        If true check if the sim box is triclinic. Default False.

    customkeys : list of strings, optional
        A list of extra keywords to read from trajectory file.

    Returns
    -------
    atoms : list of `Atom` objects
        list of all atoms as created by user input

    boxdims : list of list of floats
        The dimensions of the box. This is of the form `[[xlo, xhi],[ylo, yhi],[zlo, zhi]]` where `lo` and `hi` are
        the upper and lower bounds of the simulation box along each axes. For triclinic boxes, this is scaled to
        `[0, scalar length of the vector]`.

    box : list of list of floats
        list of the type `[[x1, x2, x3], [y1, y2, y3], [zz1, z2, z3]]` which are the box vectors. Only returned if
        `box_vectors` is set to True.

    triclinic : bool
        True if the box is triclinic. Only returned if `check_triclinic` is set to True

    .. note::

        Values are always returned in the order `atoms, boxdims, box, triclinic` if all
        return keywords are selected. For example, ff `check_triclinic` is not selected, the return
        values would still preserve the order and fall back to  `atoms, boxdims, box`.

    Notes
    -----
    Read a lammps-dump style snapshot that can have variable headers, reads in type and so on.
    Zipped files which end with a `.gz` can also be read automatically. However, if the file does not
    end with a `.gz` extension, keyword `compressed = True` can also be used.

    Examples
    --------
    >>> atoms, box = read_lammps_dump('conf.dump')
    >>> atoms, box = read_lammps_dump('conf.dump.gz')
    >>> atoms, box = read_lammps_dump('conf.d', compressed=True)

    """
    if isinstance(infile, list):
        islist = True
        f = infile
    else:
        islist = False

    if not islist:
        if not os.path.exists(infile):
            raise FileNotFoundError("Filename %s not found"%infile)
        #first depending on if the extension is .gz - use zipped read
        raw = infile.split('.')
        if raw[-1] == 'gz' or  compressed:
            f = gzip.open(infile,'rt')
        else:
            f = open(infile,'r')

    if customkeys == None:
        customkeys = []

    #now go through the file line by line
    paramsread = False
    atoms = []
    triclinic = False
    volume_fraction = 1.00

    #now if custokeys are provided - read those in too
    customread = False
    customlength = len(customkeys)
    if customlength > 0:
        customread = True

    nblock = 0
    for count, line in enumerate(f):
        #print(count, line)
        if not paramsread:
            #atom numer is at line 3
            if count == 3:
                natoms = int(line.strip())
                nblock = natoms+9
            #box dims in lines 5,6,7
            elif count == 5:
                raw = line.strip().split()
                boxx = [float(raw[0]), float(raw[1])]
                if len(raw) == 3:
                    xy = float(raw[2])
            elif count == 6:
                raw = line.strip().split()
                boxy = [float(raw[0]), float(raw[1])]
                if len(raw) == 3:
                    xz = float(raw[2])
            elif count == 7:
                raw = line.strip().split()
                boxz = [float(raw[0]), float(raw[1])]
                if len(raw) == 3:
                    yz = float(raw[2])
                    triclinic = True
                    tilts = [xy, xz, yz]
                #boxdims = [boxx, boxy, boxz]

            #header is here
            elif count == 8:
                raw = line.strip().split()
                headerdict = { raw[x]:x-2 for x in range(0, len(raw)) }
                paramsread = True
                if customread:
                    if not np.prod([(x in headerdict) for x in customkeys]):
                        raise KeyError("some values in custokeys was not found in the file")
                #add a new keyword for scaled coordinates
                if "x" in headerdict.keys():
                    scaled = False
                elif "xs" in headerdict.keys():
                    scaled = True
                    headerdict["x"] = headerdict.pop("xs")
                    headerdict["y"] = headerdict.pop("ys")
                    headerdict["z"] = headerdict.pop("zs")
                else:
                    raise ValueError("only x/xs, y/ys andz/zs keys are allowed for traj file")


        else:
            if count == nblock:
                break
            raw = line.strip().split()
            idd = int(raw[headerdict["id"]])
            typ = int(raw[headerdict["type"]])
            x = float(raw[headerdict["x"]])
            y = float(raw[headerdict["y"]])
            z = float(raw[headerdict["z"]])

            atom = pca.Atom()
            atom.pos = [x, y, z]
            atom.id = idd
            atom.type = typ
            atom.loc = count-8

            customdict = {}
            #if customkeys need to be read, do it
            if customread:
                for cc, kk in enumerate(customkeys):
                    customdict[kk] = raw[headerdict[kk]]

            atom.custom = customdict
            atoms.append(atom)

    #close files
    if not islist:
        f.close()

    if triclinic:
        #process triclinic box
        amin = min([0.0, tilts[0], tilts[1] ,tilts[0]+tilts[1]])
        amax = max([0.0, tilts[0], tilts[1] ,tilts[0]+tilts[1]])
        bmin = min([0.0, tilts[2]])
        bmax = max([0.0, tilts[2]])
        xlo = boxx[0] - amin
        xhi = boxx[1] - amax
        ylo = boxy[0] - bmin
        yhi = boxy[1] - bmax
        zlo = boxz[0]
        zhi = boxz[1]

        #triclinic cell
        a = np.array([xhi-xlo, 0, 0])
        b = np.array([tilts[0], yhi-ylo, 0])
        c = np.array([tilts[1], tilts[2], zhi-zlo])

        rot = np.array([a, b, c]).T
        rotinv = np.linalg.inv(rot)
        ortho_origin = np.array([boxx[0], boxy[0], boxz[0]])

        for atom in atoms:
            #correct zero of the atomic positions (shift box to origin)
            dist = np.array(atom.pos) - ortho_origin
            atom.pos = dist

        #finally change boxdims - to triclinic box size
        box = np.array([a, b, c])
    else:
        box = np.array([[boxx[1]-boxx[0], 0, 0],[0, boxy[1]-boxy[0], 0],[0, 0, boxz[1]-boxz[0]]])

    #adjust for scled coordinates
    if scaled:
        for atom in atoms:
            dist = atom.pos
            ndist = dist[0]*box[0] + dist[1]*box[1] + dist[2]*box[2]
            atom.pos = ndist

    if check_triclinic:
        return atoms, box, triclinic
    else:
        return atoms, box

def write_snap(sys, outfile, compressed = False, 
    customkeys=None, customvals=None, timestep=0):
    """
    Write the state of the system to a trajectory file.

    Parameters
    ----------
    sys : `System` object
        the system object to be written out

    outfile : string
        name of the output file

    compressed : bool, default false
        write a `.gz` format

    customkey : string or list of strings, optional
        If specified, it adds this custom column to the dump file. Default None.

    customvals : list or list of lists, optional
        If `customkey` is specified, `customvals` take an array of the same length
        as number of atoms, which contains the values to be written out.
        shape: natoms x ncustomkeys
    
    timestep: int, optional
        Specify the timestep value, default 0

    Returns
    -------
    None

    """
    if customkeys == None:
        customkeys = []

    box = sys.box
    boxx = np.sqrt(np.sum(np.array(box[0])**2))
    boxy = np.sqrt(np.sum(np.array(box[1])**2))
    boxz = np.sqrt(np.sum(np.array(box[2])**2))

    atoms = sys.atoms

    if len(customkeys) > 0:
        if customvals is None:
            cvals = [sys.get_custom(atom, customkeys) for atom in atoms]
        else:
            #first check if dim is equal to keys dim
            shape = np.array(customvals).shape
            rqdshape = (len(atoms), len(customkeys))
            if shape != rqdshape:
                raise ValueError("Customvals should be of shape natoms x ncustomkeys. Found %d-%d, should be %d-%d"%(shape[0], 
                    shape[1], rqdshape[0], rqdshape[1]))
            cvals = customvals

    #open files for writing
    if isinstance(outfile, io.IOBase):
        dump = outfile
    else:
        if compressed:
            gz = gzip.open(outfile,'wt')
            dump = gz
        else:
            gz = open(outfile,'w')
            dump = gz

    #now write
    dump.write("ITEM: TIMESTEP\n")
    dump.write("%d\n" % timestep)
    dump.write("ITEM: NUMBER OF ATOMS\n")
    dump.write("%d\n" % len(atoms))
    dump.write("ITEM: BOX BOUNDS\n")
    dump.write("%f %f\n" % (0, boxx))
    dump.write("%f %f\n" % (0, boxy))
    dump.write("%f %f\n" % (0, boxz))

    #now write header
    if len(customkeys) > 0:
        ckey = " ".join(customkeys)
        title_str = "ITEM: ATOMS id type x y z %s\n"% ckey
    else:
        title_str = "ITEM: ATOMS id type x y z\n"

    dump.write(title_str)

    for cc, atom in enumerate(atoms):
        pos = atom.pos
        if len(customkeys) > 0:
            cval_atom = " ".join(np.array(list(cvals[cc])).astype(str))
            atomline = ("%d %d %f %f %f %s\n")%(atom.id, atom.type, pos[0], pos[1], pos[2], cval_atom)
        else:
            atomline = ("%d %d %f %f %f\n")%(atom.id, atom.type, pos[0], pos[1], pos[2])

        dump.write(atomline)

    if not isinstance(outfile, io.IOBase):
        dump.close()


def split_snaps(infile, compressed = False,makedir=True):
    """
    Read in a LAMMPS dump trajectory file and convert it to individual time slices.

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
            if count == 3:
                natoms = int(line.strip())
                break
    f.close()

    #now restart f()
    if raw[-1] == 'gz' or  compressed:
        f = gzip.open(infile,'rt')
    else:
        f = open(infile,'r')


    nblock = natoms+9
    startblock = 0
    count=1
    snaps = []



    for line in f:
        if(count==1):
            ff = newdir+'/'+".".join([infile, 'snap', str(startblock), 'dat'])
            lines = []
            lines.append(line)

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
        count+=1

    f.close()

    return snaps

def convert_snap(**kwargs):
    raise NotImplementedError("convert method for mdtraj is not implemented")
