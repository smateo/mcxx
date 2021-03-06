! <testinfo>
! test_generator=config/mercurium-omp
! </testinfo>
PROGRAM MAIN

 IMPLICIT NONE
 INTEGER, ALLOCATABLE :: A(:), B(:, :), C(:, :, :)

 ALLOCATE(A(10))
 ALLOCATE(B(2:20, 3:30))
 ALLOCATE(C(4:40, 5:50, 6:60))

 !$OMP PARALLEL PRIVATE(A) FIRSTPRIVATE(B) SHARED(C)
 PRINT *, LBOUND(A, DIM=1), UBOUND(A, DIM=1)
 PRINT *, LBOUND(B, DIM=1), UBOUND(B, DIM=1) &
        , LBOUND(B, DIM=2), UBOUND(B, DIM=2)
 PRINT *, LBOUND(C, DIM=1), UBOUND(C, DIM=1) &
        , LBOUND(C, DIM=2), UBOUND(C, DIM=2) &
        , LBOUND(C, DIM=3), UBOUND(C, DIM=3)

 IF (LBOUND(A, DIM=1) /= 1) STOP 1
 IF (UBOUND(A, DIM=1) /= 10) STOP 10

 IF (LBOUND(B, DIM=1) /= 2) STOP 2
 IF (UBOUND(B, DIM=1) /= 20) STOP 20

 IF (LBOUND(B, DIM=2) /= 3) STOP 3
 IF (UBOUND(B, DIM=2) /= 30) STOP 30

 IF (LBOUND(C, DIM=1) /= 4) STOP 4
 IF (UBOUND(C, DIM=1) /= 40) STOP 40

 IF (LBOUND(C, DIM=2) /= 5) STOP 5
 IF (UBOUND(C, DIM=2) /= 50) STOP 50

 IF (LBOUND(C, DIM=3) /= 6) STOP 6
 IF (UBOUND(C, DIM=3) /= 60) STOP 60

 !$OMP END PARALLEL

END PROGRAM MAIN
