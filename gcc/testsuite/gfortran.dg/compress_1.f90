! { dg-do run }
! { dg-set-target-env-var GFORTRAN_COMPRESS "zstd:3:byte" }
! Blosc compression of unformatted sequential files (GFORTRAN_COMPRESS).
! Round trip of mixed records, partial consumption of a chunk by several
! items, zero-size items, a record larger than one blosc block, REWIND and
! BACKSPACE.  Passes unchanged when libgfortran is built without blosc.
program compress_1
  implicit none
  type :: t
     integer :: tag
     real(8) :: v(3)
     character(len=12) :: name
  end type t
  integer, parameter :: n = 400000
  real(8), allocatable :: a(:), ra(:)
  real(8) :: s1, s2, s3, r3(3)
  integer :: i, k, m(5), rm(5), rk, ios
  integer, allocatable :: empty(:), rempty(:)
  complex(8) :: z(10), rz(10)
  character(len=20) :: c, rc
  type(t) :: d, rd
  logical :: l(4), rl(4)
  integer(8) :: sz

  allocate (a(n), ra(n), empty(0), rempty(0))
  a = [(sin (real (i, 8) * 1d-3), i = 1, n)]
  m = [1, 2, 3, 4, 5]
  z = [(cmplx (i, -i, 8), i = 1, 10)]
  c = 'hello compressed'
  d = t (7, [0.5d0, 1.5d0, 2.5d0], 'restart     ')
  l = [.true., .false., .true., .false.]

  open (10, file='compress_1.dat', form='unformatted', status='replace')
  write (10) a                         ! one large chunk (3.2 MB)
  write (10) m, z, c                   ! several items in one record
  write (10) k_dummy (), empty, d, l   ! zero-size item in the middle
  write (10) a(1:3)
  close (10)

  open (10, file='compress_1.dat', form='unformatted', status='old')
  read (10) ra
  if (any (ra /= a)) stop 1
  read (10) rm, rz, rc
  if (any (rm /= m) .or. any (rz /= z) .or. rc /= c) stop 2
  read (10) rk, rempty, rd, rl
  if (rk /= 42 .or. rd%tag /= 7 .or. any (rd%v /= d%v) .or. rd%name /= d%name &
      .or. any (rl .neqv. l)) stop 3
  read (10) s1, s2, s3                 ! three scalars from a 3-element chunk
  if (s1 /= a(1) .or. s2 /= a(2) .or. s3 /= a(3)) stop 4
  read (10, iostat=ios) s1
  if (ios >= 0) stop 5                 ! must be end of file

  rewind (10)
  read (10) ra
  if (any (ra /= a)) stop 6
  read (10) rm
  backspace (10)
  read (10) rm, rz, rc
  if (any (rm /= m) .or. any (rz /= z) .or. rc /= c) stop 7
  read (10) rk, rempty, rd, rl
  read (10) r3
  if (any (r3 /= a(1:3))) stop 8
  inquire (10, size=sz)
  close (10, status='delete')
contains
  integer function k_dummy ()
    k_dummy = 42
  end function k_dummy
end program compress_1
