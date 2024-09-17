//include libraries
#include <CL/sycl.hpp>
#include <iostream>
#include <chrono>
#include <vector>

//using namespace sycl for DPC++
using namespace sycl;

//determine variable to test non-square matrix
constexpr size_t M = 2000;
constexpr size_t N = 1000;
constexpr size_t P = 1500;
//TILE_SIZE to determine how much to caclucate at one time
constexpr size_t TILE_SIZE = 10; // This should be optimized based on the hardware. //8 works for subgroups any number other than 16 works 64 is maximum without error

//function definitiion with parameters with pointers to variables defined in main function
void matrix_multiplication(const float* A, const float* B, float* C, queue& q) {
    //buffers to hold data
    buffer<float, 2> bufA(A, range<2>(M, N));
    buffer<float, 2> bufB(B, range<2>(N, P));
    buffer<float, 2> bufC(C, range<2>(M, P));
    q.submit([&](handler& h) {
        //accessors to allow access to data inside kernel and denote permissions (read or write)
        auto accA = bufA.get_access<access::mode::read>(h);
        auto accB = bufB.get_access<access::mode::read>(h);
        auto accC = bufC.get_access<access::mode::write>(h);

        //parallel for loop with kernel name matrix_kernel
        h.parallel_for<class matrix_kernel>(range<2>(M, P), [=](id<2> idx) {
            //declare variables
            const int id_0 = idx[0];
            const int id_1 = idx[1];
            //flat variable to hold temporary data with f to signify that it is a precise floating-point number
            float temp = 0.0f;
            //for loop to loop over common dimension and correspond the above variables to accA and accB respectively
            //and for each iteration the product of accC, id_0 and k is calculated by storing in the temp variable
            for (int k = 0; k < N; ++k) {
                //loops over temp variable  making it equal to accA and accB at specific points
                temp += accA[id_0][k] * accB[k][id_1];
            }
            //makes accessor C equal to temp
            accC[id_0][id_1] = temp;
            });
        });
}


void tiled_matrix_multiplication(const float* A, const float* B, float* C, queue& q) {

    //buffers to hold data
    buffer<float, 2> bufA(A, range<2>(M, N));
    buffer<float, 2> bufB(B, range<2>(N, P));
    buffer<float, 2> bufC(C, range<2>(M, P));

    //submit to queue with handler h
    q.submit([&](handler& h) {
        //accessors to denote permissions and access to buffers inside the kernel.
        auto accA = bufA.get_access<access::mode::read>(h);
        auto accB = bufB.get_access<access::mode::read>(h);
        auto accC = bufC.get_access<access::mode::write>(h);

        //create local acceessors only shared in work group.
        accessor<float, 2, access::mode::read_write, access::target::local> tileA(range<2>(TILE_SIZE, TILE_SIZE), h);
        accessor<float, 2, access::mode::read_write, access::target::local> tileB(range<2>(TILE_SIZE, TILE_SIZE), h);

        //parallel for loop to specify the NC-range, stating both global and local size to be a 2D range, locally with TILE_SIZE for rows and columns.
        //and globally with M rows and P columns
        h.parallel_for<class tiled_matrix_multiplication>(nd_range<2>(range<2>(M, P), range<2>(TILE_SIZE, TILE_SIZE)), [=](nd_item<2> item) {
            //values set as const to stop them being changed after initialisation, function to get global and local work-items IDs respectively.
            const int global_row = item.get_global_id(0);
            const int global_column = item.get_global_id(1);
            const int local_row = item.get_local_id(0);
            const int local_column = item.get_local_id(1);
            //float variable to hold product sum
            float temp = 0.0f;

            //for loop to iterate through and perform the multiplication
            for (int t = 0; t < (int)N; t += TILE_SIZE) {
                //caclulate tile width
                int localWidth = min(TILE_SIZE, N - t);
                //loads tile belonging to matrix A into local memory
                if (global_row < M && (t + local_column) < N) {
                    tileA[local_row][local_column] = accA[global_row][t + local_column];
                }
                else {
                    //else initialises variables with 0
                    tileA[local_row][local_column] = 0.0f;
                }
                //loads tile of matrix B into local memory
                if ((t + local_row) < N && global_column < P) {
                    tileB[local_row][local_column] = accB[t + local_row][global_column];
                }
                //else initalises with 0
                else {
                    tileB[local_row][local_column] = 0.0f;
                }
                //synchronise threads to check tile loading is complete
                item.barrier(access::fence_space::local_space);
                //actual matrix multiplication
                for (int k = 0; k < localWidth; ++k) {
                    temp += tileA[local_row][k] * tileB[k][local_column];
                }
                //synchronise threads again
                item.barrier(access::fence_space::local_space);
            }
            //write back to global memory
            if (global_row < M && global_column < P) {
                accC[global_row][global_column] = temp;
            }
            });
        });
}


void i_usm_matrix_multiplication(const float* A, const float* B, float* C, queue& q) {
    //allocate memory for matrix calculations
    float* dA = malloc_shared<float>(M * N, q);
    float* dB = malloc_shared<float>(N * P, q);
    float* dC = malloc_shared<float>(M * P, q);

    // Copy matrix A and B to chared memory
    std::memcpy(dA, A, sizeof(float) * M * N);
    std::memcpy(dB, B, sizeof(float) * N * P);
    //initalises matrix C
    std::memset(dC, 0, sizeof(float) * M * P);

    //kernel for multiplication
    q.submit([&](handler& h) {
        //parallel_for loop for asynchronous calculation, in a 2 
        h.parallel_for<class i_USM_mul_kernel>(range<2>(M, P), [=](id<2> idx) {
            //row index for result 
            size_t i = idx[0];
            //column index for the result
            size_t j = idx[1];
            //float variable to hold product result
            float temp = 0.0f;
            //computer product of row i of matrix A and column J of matrix B
            for (size_t k = 0; k < N; ++k) {
                temp += dA[i * N + k] * dB[k * P + j];
            }
            //store value in result matrix c
            dC[i * P + j] = temp;
            });
        });
    q.wait();

    // Copy back to host
    std::memcpy(C, dC, sizeof(float) * M * P);

    free(dA, q);
    free(dB, q);
    free(dC, q);
}


void e_usm_matrix_multiplication(const float* A, const float* B, float* C, queue& q) {
    //allocates memory for matrix
    float* dA = malloc_device<float>(M * N, q);
    float* dB = malloc_device<float>(N * P, q);
    float* dC = malloc_device<float>(M * P, q);

    //copy data from host to device for matrix B and A
    q.memcpy(dA, A, sizeof(float) * M * N).wait();
    q.memcpy(dB, B, sizeof(float) * N * P).wait();
    //initialises matrix C to 0
    q.memset(dC, 0, sizeof(float) * M * P).wait();

    //kernel for matrix multiplicatoin
    q.submit([&](handler& h) {
        //define parallel_for loop 
        h.parallel_for<class explicit_USM_mul_kernel>(range<2>(M, P), [=](id<2> idx) {
            //row index for result matrix
            size_t i = idx[0];
            //column index for result matrix
            size_t j = idx[1];
            //temporary float to hold product
            float temp = 0.0f;
            //compute elements of result matriz
            for (size_t k = 0; k < N; ++k) {
                temp += dA[i * N + k] * dB[k * P + j];
            }

            //store value in the result matrix
            dC[i * P + j] = temp;
            });
        });
    //wait for queue to complete
    q.wait();
    //copy result matrix to host
    q.memcpy(C, dC, sizeof(float) * M * P).wait();
    //free memory
    free(dA, q);
    free(dB, q);
    free(dC, q);
}
void subgroup_matrix_multiplication(const float* A, const float* B, float* C, queue& q) {
    //create buffers for matrices A B and C
    buffer<float, 2> bufA(A, range<2>(M, N));
    buffer<float, 2> bufB(B, range<2>(N, P));
    buffer<float, 2> bufC(C, range<2>(M, P));
    //submits to queue to run code on kernel
    q.submit([&](handler& h) {
        //creates accessors to the buffers with differing permissions
        auto accA = bufA.get_access<access::mode::read>(h);
        auto accB = bufB.get_access<access::mode::read>(h);
        auto accC = bufC.get_access<access::mode::write>(h);
        //creates local accessors for tihis work group
        accessor<float, 2, access::mode::read_write, access::target::local> tileA(range<2>(TILE_SIZE, TILE_SIZE), h);
        accessor<float, 2, access::mode::read_write, access::target::local> tileB(range<2>(TILE_SIZE, TILE_SIZE), h);
        //kernel for matrix multiplication
        h.parallel_for<class SubgroupMatrixMul>(nd_range<2>(range<2>(M, P), range<2>(TILE_SIZE, TILE_SIZE)), [=](nd_item<2> item) {
            //gets local and global rows and columnbs respectivly
            int localRow = item.get_local_id(0);
            int localCol = item.get_local_id(1);
            int globalRow = item.get_global_id(0);
            int globalCol = item.get_global_id(1);
            //sub group to share data!
            const auto sg = item.get_sub_group();
            //temp variable for storing results
            float temp = 0.0f;
            //loops over tiles of the matrix
            for (int t = 0; t < N; t += TILE_SIZE) {
                // Load tile A into local memory as long as in bound
                if (globalRow < M && (t + localCol) < N) {
                    tileA[localRow][localCol] = accA[globalRow][t + localCol];
                }
                //else loads tile A into global memory
                else {
                    tileA[localRow][localCol] = 0.0f;
                }
                //loads tile B into local memory, as long as in set bounds of matrix as long as
                if ((t + localRow) < N && globalCol < P) {
                    tileB[localRow][localCol] = accB[t + localRow][globalCol];
                }
                //tile B laoded into global memory
                else {
                    tileB[localRow][localCol] = 0.0f;
                }
                // barrier to wait for all items in the work group
                item.barrier(access::fence_space::local_space);
                //tempt variable holds value of corresponding tile matrices
                for (int k = 0; k < TILE_SIZE; k++) {
                    temp += tileA[localRow][k] * tileB[k][localCol];
                }
                //sub group barrier
                sg.barrier();
            }
            //checks if final matrix is in bounds and then stores it in matrix C
            if (globalRow < M && globalCol < P) {
                accC[globalRow][globalCol] = temp;
            }
            });
        });
}


int main() {

    //declare and initialise matrices A B and C
    std::vector<float> A(M * N), B(N * P), C(M * P);

    // Initialize matrices A with values where each element is 1 times row index
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            A[i * N + j] = 1.0f * (i + 1);
        }
    }

    //initialise matrices B where each element is 1 times column index
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < P; j++) {
            B[i * P + j] = 1.0f * (j + 1);
        }
    }

    //creates queue to run on chosen device
    queue q;
    // writes out what device the program is running on
    std::cout << "Running on " << q.get_device().get_info<info::device::name>() << std::endl;
    //start clock
    auto start = std::chrono::high_resolution_clock::now();

    // Select which function to run:
     //matrix_multiplication(A.data(), B.data(), C.data(), q);
     //i_usm_matrix_multiplication(A.data(), B.data(), C.data(), q);
    // e_usm_matrix_multiplication(A.data(), B.data(), C.data(), q);
     //tiled_matrix_multiplication(A.data(), B.data(), C.data(), q);
    subgroup_matrix_multiplication(A.data(), B.data(), C.data(), q);
    //wait for all operations in queue to finish
    q.wait();
    //end clock
    auto end = std::chrono::high_resolution_clock::now();
    //work out time taken for funciton
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    //write out time
    std::cout << "Matrix multiplication completed in " << duration.count() << " ms\n";
    //limit number of rows to display
    int display_rows = std::min(10, static_cast<int>(M));
    //limit number of columns to display
    int display_cols = std::min(10, static_cast<int>(P));
    //writes out some of matrix C
    std::cout << "Part of the resultant matrix C:\n";
    for (int i = 0; i < display_rows; i++) {
        for (int j = 0; j < display_cols; j++) {
            std::cout << C[i * P + j] << "\t";
        }
        std::cout << "\n";
    }
    //return 0!
    return 0;
}