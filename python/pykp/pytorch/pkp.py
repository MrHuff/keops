import os.path
import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)) + os.path.sep + '..')

from pykp import cudaconv,cudagradconv
import torch
import numpy

class Hamiltonian(torch.autograd.Function):
    """ This class implement the Hamiltonian (used in the LDDMM formulation).
        It may be used with the pytorch autograd lib. 

        It uses the 'libkp' to perform the computation with cuda.
        Author: b.
    """
    # Computation are made in float32
    dtype = torch.FloatTensor 

    def forward(self, q, p, sigma):
        # save everything to compute the gradient
        self.qq = q
        self.pp = p
        self.sig = sigma
        # init gamma which contains the output of the convolution K_qq @ p
        self.gamma  = torch.Tensor(p.numel()).type(dtype)
        # Joyeuseté : une fois passé en argument d'une fonction les Tensor perdent leur caractere Variable :
        cudaconv.cuda_conv(q.numpy(),q.numpy(),p.numpy(),self.gamma.numpy(),sigma.numpy()[0]) 
        # p.view(-1) = "p.ravel()" : Ham(q,p) = p^T @ K_qq @ p
        return torch.Tensor([p.view(-1).dot(self.gamma)])

    def backward(self,e):
        # init gammagrad which contains the output of the grad_convolution
        gammagrad = torch.Tensor(self.qq.numel()).type(dtype) #0d array
        cudagradconv.cuda_gradconv(self.pp.numpy(),self.qq.numpy(),self.qq.numpy(),self.pp.numpy(),gammagrad.numpy(),self.sig.numpy()[0])
        n = self.qq.size(0)
        d = self.qq.size(1)
                                   
        return 2* gammagrad.view(n,d)*e, 2 * self.gamma.view(n,d)*e, None

#--------------------------------------------------#
# Init variables to get a minimal working example:
#--------------------------------------------------#
dtype = torch.FloatTensor

n =15
d=3

q0 = .6 * torch.linspace(0,5,n*d).type(dtype).view(n,d)
q0 = torch.autograd.Variable(q0, requires_grad = True)

p0 = .6 * torch.linspace(-.2,.2,n*d).type(dtype).view(n,d)
p0 = torch.autograd.Variable(p0, requires_grad = True)

s = torch.Tensor([2.5]).type(dtype)
s = torch.autograd.Variable(s, requires_grad = False)

#--------------------------------------------------#
# check the class Hamiltonian
#--------------------------------------------------#
ham = Hamiltonian()
h = ham(q0,p0,s)
dq = torch.autograd.grad(ham(q0,p0,s),q0,create_graph = True)[0]
dp = torch.autograd.grad(ham(q0,p0,s),p0,create_graph = True)[0]

gc = torch.autograd.gradcheck(ham, inputs=(q0,p0,s) , eps=4e-3, atol=1e-5, rtol=1e-3 )
print('Gradcheck for Hamiltonian: ',gc)
print('\n')

#--------------------------------------------------#
# check that we are able to compute derivatives with autograd
#--------------------------------------------------#
q1 = .6 * torch.linspace(0,5,n*d).type(dtype).view(n,d)
q1 = torch.autograd.Variable(q1, requires_grad = True)

p1 = .5 * torch.linspace(-.2,.2,n*d).type(dtype).view(n,d)
p1 = torch.autograd.Variable(p1, requires_grad = True)
gg = torch.sin(ham(q1,p1,s))
gg.backward()
print('derivative of sin(ham): ',p1.grad)

#--------------------------------------------------#
# check that we are able to compute derivatives with autograd
#--------------------------------------------------#
q1 = .6 * torch.linspace(0,5,n*d).type(dtype).view(n,d)
q1 = torch.autograd.Variable(q1, requires_grad = True)

p1 = .5 * torch.linspace(-.2,.2,n*d).type(dtype).view(n,d)
p1 = torch.autograd.Variable(p1, requires_grad = True)
sh = torch.sin(ham(q1,p1,s))

gsh = torch.autograd.grad( sh , p1, create_graph = True)[0]
print('derivative of sin(ham): ', gsh)
print('Is gsh volatile ? ',gsh.volatile)