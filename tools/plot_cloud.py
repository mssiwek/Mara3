#!/usr/bin/env python3



import argparse
import numpy as np
import h5py
import matplotlib.pyplot as plt



def plot_single_file(filename):
    fig, (axes, cb_axes) = plt.subplots(nrows=2, ncols=4, figsize=[15, 8], gridspec_kw={'height_ratios': [19, 1]})
    h5f = h5py.File(filename, 'r')

    r = h5f['radial_vertices'][...]
    q = h5f['polar_vertices'][...]
    d = h5f['mass_density'][...]
    p = h5f['gas_pressure'][...]
    u = h5f['radial_gamma_beta'][...]
    dL = h5f['radial_energy_flow'][...]
    dO = h5f['solid_angle_at_theta'][...]
    r /= r[0]

    R, Q = np.meshgrid(r, q)
    X = np.log10(R) * np.cos(Q)
    Y = np.log10(R) * np.sin(Q)

    m0 = axes[0].pcolormesh(Y, X, np.log10(d.T))
    m1 = axes[1].pcolormesh(Y, X, np.log10(p.T))
    m2 = axes[2].pcolormesh(Y, X, np.log10(u.T), vmin=-3, vmax=2)
    m3 = axes[3].pcolormesh(Y, X, np.log10(dL / dO).T)

    axes[0].set_title(r'$\log_{10}(\rho)$')
    axes[1].set_title(r'$\log_{10}(p)$')
    axes[2].set_title(r'$\log_{10}(\Gamma \beta_r)$')
    axes[3].set_title(r'Luminosity $dL / d\Omega$')

    for m, cax in zip([m0, m1, m2, m3], cb_axes):
        fig.colorbar(m, cax=cax, orientation='horizontal')

    for ax in axes:
        ax.set_aspect('equal')
        ax.set_xticks([])
        if ax is not axes[0]:
            ax.set_yticks([])

    axes[0].set_ylabel(r'$\log_{10}(r)$')

    fig.subplots_adjust(left=0.05, right=0.95, top=0.95, bottom=0.05, wspace=0.1, hspace=0.0)
    fig.suptitle(filename)

    return fig



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filenames", nargs='+')

    args = parser.parse_args()

    for filename in args.filenames:
        plot_single_file(filename)

    plt.show()
