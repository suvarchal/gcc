! { dg-do run }
! { dg-additional-options "-pthread" }
! { dg-set-target-env-var GFORTRAN_COMPRESS "yes" }
! { dg-set-target-env-var GFORTRAN_COMPRESS_LAYOUT "nd" }
! { dg-set-target-env-var GFORTRAN_ASYNC_COPY "yes" }
! Shape-aware array items (GFORTRAN_COMPRESS_LAYOUT=nd): arrays of a
! compressed sequential file become c-blosc2 b2nd frames with shape and
! type, scalars stay in byte chunks, both mixed in one record.  Reading
! into a differently shaped array of the same size works; a size mismatch
! is an error.  Also through asynchronous copy mode.  Passes unchanged
! without c-blosc2.
program compress_nd_1
  implicit none
  real(8), allocatable :: t(:,:), rt(:,:), rflat(:)
  integer, allocatable :: k3(:,:,:), rk3(:,:,:)
  real(8) :: big(20, 30), rs(10, 15), x
  complex(8) :: z(7), rz(7)
  logical :: l(5), rl(5)
  integer :: i, j, ios, u, n
  real(8), allocatable :: small(:)
  allocate (t(47, 1000), rt(47, 1000), rflat(47000), k3(4, 5, 6), rk3(4, 5, 6), small(10))
  do j = 1, 1000
     do i = 1, 47
        t(i, j) = 1000.0d0 * j + i + 0.25d0
     end do
  end do
  k3 = reshape ([(i, i = 1, 120)], [4, 5, 6])
  big = reshape ([(0.5d0 * i, i = 1, 600)], [20, 30])
  z = [(cmplx (i, -2 * i, 8), i = 1, 7)]
  l = [.true., .false., .false., .true., .true.]

  open (newunit=u, file='compress_nd_1.dat', form='unformatted', status='replace')
  write (u) 7, t, 3.5d0           ! scalar, 2-D array, scalar in one record
  write (u) k3, big(1:20:2, 1:30:2), z, l
  write (u) t                     ! read back below into a 1-D array
  write (u) t                     ! read back below with the wrong size
  close (u)

  open (newunit=u, file='compress_nd_1.dat', form='unformatted', status='old')
  read (u) n, rt, x
  if (n /= 7 .or. x /= 3.5d0 .or. any (rt /= t)) stop 1
  read (u) rk3, rs, rz, rl
  if (any (rk3 /= k3)) stop 2
  if (any (rs /= big(1:20:2, 1:30:2))) stop 3
  if (any (rz /= z) .or. any (rl .neqv. l)) stop 4
  read (u) rflat
  if (any (rflat /= reshape (t, [47000]))) stop 5
  read (u, iostat=ios) small
  if (ios == 0) then
     ! Without c-blosc2 the file is uncompressed and reading a prefix of
     ! a record is legal; with b2nd items it is an error.
     if (small(1) /= t(1, 1)) stop 6
  end if
  close (u, status='delete')

  ! Asynchronous copy mode (GFORTRAN_ASYNC_COPY): the I/O thread writes
  ! the frames from its copy, so overwriting t before WAIT is allowed.
  open (newunit=u, file='compress_nd_1.dat', form='unformatted', status='replace', &
        asynchronous='yes')
  write (u, asynchronous='yes') t
  t = -1.0d0
  wait (u)
  close (u)
  open (newunit=u, file='compress_nd_1.dat', form='unformatted', status='old')
  read (u) rt
  close (u, status='delete')
  if (rt(47, 1000) /= 1000.0d0 * 1000 + 47 + 0.25d0) stop 7
end program compress_nd_1
