# Real2Sim FEM
'Real2Sim: Visco-elastic parameter estimation from dynamic motion' source code, modified by joo yong sim

It is not still working.. almost 70% of cpp are compiled.
The current error is 
![image](https://github.com/user-attachments/assets/f672e16c-0a52-4030-889d-4ca324e46c59)


# Environment 
- vtk9
- spectra-git-v0.9.0 (issues with later version, so I used this version)
- LBFGSpp-git-v0.3.0 (this was the latest version currently)
- Eigen version: 3.4.0 

Sources for the paper Real2Sim: Visco-elastic parameter estimation from dynamic motion (and partially also ADD: Analytically Differentiable Dynamics for Multi-Body Systems with Frictional Contact)
(https://dl.acm.org/doi/10.1145/3355089.3356548 and https://dl.acm.org/doi/abs/10.1145/3414685.3417766)

# External libraries required:
Eigen (eigen.tuxfamily.org/)
VTK (https://vtk.org/)
LBFGSpp (https://github.com/yixuan/LBFGSpp)
Optimlib (https://github.com/kthohr/optim)
Spectra (https://github.com/yixuan/spectra)
MMG (https://www.mmgtools.org/)

(Depending on the application, only Eigen and VTK, and one of LBFGSpp or Optimlib might be sufficient if relevant parts are excluded from the build, but the code it not automatically configured to do this.)
