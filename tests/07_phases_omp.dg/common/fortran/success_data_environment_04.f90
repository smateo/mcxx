! <testinfo>
! test_generator=config/mercurium-omp
! </testinfo>
SUBROUTINE S1(P, S)
    IMPLICIT NONE
    INTEGER, EXTERNAL :: OMP_GET_THREAD_NUM, OMP_GET_MAX_THREADS
    INTEGER :: S
    EXTERNAL :: P

    !$OMP PARALLEL
    CALL S2(P, OMP_GET_THREAD_NUM(), S)
    !$OMP END PARALLEL

    PRINT *, "MAX_THREADS=", OMP_GET_MAX_THREADS(), "S=", S
    IF (S /= OMP_GET_MAX_THREADS()) STOP 1
END SUBROUTINE S1

SUBROUTINE S2(P, I, S)
    EXTERNAL :: P
    INTEGER :: S

    CALL P(I, S)
END SUBROUTINE S2

SUBROUTINE S3(X, S)
    INTEGER :: X, S

    PRINT *, "X = ", X

    !$OMP ATOMIC
    S = S + 1
END SUBROUTINE S3

PROGRAM MAIN
    EXTERNAL :: S3
    INTEGER :: S
    S = 0

    CALL S1(S3, S)
END PROGRAM MAIN
