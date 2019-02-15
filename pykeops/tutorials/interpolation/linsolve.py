
import time
import numpy as np
import torch
from pykeops.numpy import Genred as GenredNumpy
from pykeops.torch import Genred as GenredTorch

def ConjugateGradientSolver(linop,b,eps=1e-6):
    # Conjugate gradient algorithm to solve linear system of the form
    # Ma=b where linop is a linear operation corresponding
    # to a symmetric and positive definite matrix
    arraytype = type(b)
    copy = np.copy if arraytype == np.ndarray else torch.clone
    a = 0
    r = copy(b)
    p = copy(r)
    nr2 = (r**2).sum()
    k = 0
    while True:
        Mp = linop(p)
        alpha = nr2/(p*Mp).sum()
        a += alpha*p
        r -= alpha*Mp
        nr2new = (r**2).sum()
        if nr2new < eps**2:
            break
        p = r + (nr2new/nr2)*p
        nr2 = nr2new
        k += 1
    print("numiters=",k)
    return a

class InvLinOp(torch.autograd.Function):
    @staticmethod
    def forward(ctx, linop, b):
        a = ConjugateGradientSolver(linop,b,eps=1e-6)
        ctx.save_for_backward(a)
        return a
    @staticmethod
    def backward(ctx, grad_output):
        a, = ctx.saved_tensors
        return -linop(grad_output)

def KernelLinearSolver(K,x,b,lmbda=0,eps=1e-6,precond=False,precondKernel=None):

    arraytype = type(b)

    if arraytype == np.ndarray:
        backend = np
        dtype = x.dtype.name
        copy = np.copy
        tile = np.tile
        solve = np.linalg.solve
        norm = np.linalg.norm
        Genred = GenredNumpy
        rand = lambda m, n : np.random.rand(m,n).astype(dtype)
        randn = lambda m, n : np.random.randn(m,n).astype(dtype)
        zeros = lambda shape : np.zeros(shape).astype(dtype)
        eye = lambda n : np.eye(n).astype(dtype)
        array = lambda x : np.array(x).astype(dtype)
        arraysum = np.sum
        transpose = lambda x : x.T
        numpy = lambda x : x
    elif arraytype == torch.Tensor:
        backend = torch
        torchdtype = x.dtype
        dtype = 'float32' if torchdtype==torch.float32 else 'float64'
        torchdeviceId = torch.device('cuda:0') if useGpu else 'cpu'
        KeOpsdeviceId = torchdeviceId.index  # id of Gpu device (in case Gpu is  used)
        copy = torch.clone
        tile = torch.Tensor.repeat
        solve = lambda A, b : torch.gesv(b,A)[0].contiguous()
        norm = torch.norm
        Genred = GenredTorch
        rand = lambda m, n : torch.rand(m,n, dtype=torchdtype, device=torchdeviceId)
        randn = lambda m, n : torch.randn(m,n, dtype=torchdtype, device=torchdeviceId)
        zeros = lambda shape : torch.zeros(shape, dtype=torchdtype, device=torchdeviceId)
        eye = lambda n : torch.eye(n, dtype=torchdtype, device=torchdeviceId)
        array = lambda x : torch.tensor(x, dtype=torchdtype, device=torchdeviceId)
        randn = lambda m, n : torch.randn(m,n, dtype=torchdtype, device=torchdeviceId)
        arraysum = lambda x, axis=None : x.sum() if axis is None else x.sum(dim=axis)
        transpose = lambda x : x.t
        numpy = lambda x : x.cpu().numpy()

    def PreconditionedConjugateGradientSolver(linop,b,invprecondop,eps=1e-6):
        # Preconditioned conjugate gradient algorithm to solve linear system of the form
        # Ma=b where linop is a linear operation corresponding
        # to a symmetric and positive definite matrix
        # invprecondop is linear operation corresponding to the inverse of the preconditioner matrix
        a = 0
        r = copy(b)
        z = invprecondop(r)
        p = copy(z)
        rz = (r*z).sum()
        k = 0
        while True:    
            alpha = rz/(p*linop(p)).sum()
            a += alpha*p
            r -= alpha*linop(p)
            if (r**2).sum() < eps**2:
                break
            z = invprecondop(r)
            rznew = (r*z).sum()
            p = z + (rznew/rz)*p
            rz = rznew
            k += 1
        print("numiters=",k)
        return a

    def NystromInversePreconditioner(K,Kspec,x,lmbda):
        N,D = x.shape
        m = int(np.sqrt(N))
        ind = np.random.choice(range(N),m,replace=False)
        u = x[ind,:]
        start = time.time()
        M = K(u,u) + Kspec(tile(u,(m,1)),tile(u,(1,m)).reshape(-1,D),x).reshape(m,m)
        end = time.time()    
        print('Time for init:', round(end - start, 5), 's')
        def invprecondop(r):
            a = solve(M,K(u,x,r))
            return (r - K(x,u,a))/lmbda
        return invprecondop

    def KernelLinOp(a):
        return K(x,x,a) + lmbda*a
        
    def GaussKernel(D,Dv,sigma):
        formula = 'Exp(-oos2*SqDist(x,y))*b'
        variables = ['x = Vx(' + str(D) + ')',  # First arg   : i-variable, of size D
                     'y = Vy(' + str(D) + ')',  # Second arg  : j-variable, of size D
                     'b = Vy(' + str(Dv) + ')',  # Third arg  : j-variable, of size Dv
                     'oos2 = Pm(1)']  # Fourth arg  : scalar parameter
        my_routine = Genred(formula, variables, reduction_op='Sum', axis=1, cuda_type=dtype)
        oos2 = array([1.0/sigma**2])
        KernelMatrix = GaussKernelMatrix(sigma)
        def K(x,y,b=None):
            if b is None:
                return KernelMatrix(x,y)
            else:
                return my_routine(x,y,b,oos2)
        return K

    def GaussKernelNystromPrecond(D,sigma):
        formula = 'Exp(-oos2*(SqDist(u,x)+SqDist(v,x)))'
        variables = ['u = Vx(' + str(D) + ')',  # First arg   : i-variable, of size D
                     'v = Vx(' + str(D) + ')',  # Second arg  : i-variable, of size D
                     'x = Vy(' + str(D) + ')',  # Third arg  : j-variable, of size D
                     'oos2 = Pm(1)']  # Fourth arg  : scalar parameter
        my_routine = Genred(formula, variables, reduction_op='Sum', axis=1, cuda_type=dtype)
        oos2 = array([1.0/sigma**2])
        KernelMatrix = GaussKernelMatrix(sigma)
        def K(u,v,x):
            return my_routine(u,v,x,oos2)
        return K

    def GaussKernelMatrix(sigma):
        oos2 = 1.0/sigma**2
        def f(x,y):
            D = x.shape[1]
            sqdist = 0
            for k in range(D):
                sqdist += (x[:,k][:,None]-transpose(y[:,k][:,None]))**2
            return backend.exp(-oos2*sqdist)
        return f
    
    if type(K)==tuple:
        if K[0]=="gaussian":
            D = K[1]
            Dv = K[2]
            sigma = K[3]
            K = GaussKernel(D,Dv,sigma)
            if precond:
                precondKernel = GaussKernelNystromPrecond(D,sigma)        

    if precond:
        invprecondop = NystromInversePreconditioner(K,precondKernel,x,lmbda)
        a = PreconditionedConjugateGradientSolver(KernelLinOp,b,invprecondop,eps)
    else:
        a = ConjugateGradientSolver(KernelLinOp,b,eps)
        
    return a


