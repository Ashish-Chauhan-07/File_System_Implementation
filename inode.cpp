#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>

using namespace std;

typedef long long ll;
typedef vector<int> vi;
typedef vector<ll> vll;
typedef vector<string> vs;
typedef vector<bool> vb;
typedef vector<vi> vvi;
typedef stack<int> si;
typedef pair<int, int> pii;
typedef pair<long long, long long> pll;
typedef vector<pii> vpii;
typedef vector<pll> vpll;

const int BLOCK_SIZE = 4096; //3906
const int DISK_BLOCKS = 131072;
const int NO_OF_INODES = 78644;
const int NO_OF_FILE_DESCRIPTORS = 32;
static bool active{false};

struct inode
{
    int filesize;
    int pointer[12];
};

struct file_to_inode_mapping
{
    char file_name[30];
    int inode_num;
};

class super_block
{
public:
    ll no_of_blocks_used_by_superblock = ceil(((float)sizeof(super_block)) / BLOCK_SIZE);
    ll no_of_blocks_used_by_file_inode_mapping = ceil(((float)sizeof(struct file_to_inode_mapping) * NO_OF_INODES) / BLOCK_SIZE);
    ll starting_index_of_inodes = no_of_blocks_used_by_superblock + no_of_blocks_used_by_file_inode_mapping;
    ll no_of_blocks_used_to_store_inodes = ceil(((float)(NO_OF_INODES * sizeof(struct inode))) / BLOCK_SIZE);
    ll starting_index_of_data_blocks = no_of_blocks_used_by_superblock + no_of_blocks_used_by_file_inode_mapping + no_of_blocks_used_to_store_inodes;
    ll total_no_of_available_blocks = DISK_BLOCKS - starting_index_of_data_blocks;
    bool inode_freelist[NO_OF_INODES];
    bool datablock_freelist[DISK_BLOCKS];
};

//Global_Declarations-----------------------------------------------------------------------
string disk_name, file_name;
struct file_to_inode_mapping file_inode_mapping_arr[NO_OF_INODES];
struct inode inode_arr[NO_OF_INODES];
unordered_map<string, ll> file_to_inode_map;
unordered_map<ll, string> inode_to_file_map;
unordered_map<ll, pll> file_descriptor_map;
unordered_map<ll, int> file_descriptor_mode_map;
vll free_inode_vector;
vll free_data_block_vector;
vll free_filedescriptor_vector;
ll openfile_count;
super_block sb;
FILE *diskptr;
//--------------------------------------------------------------------------------------------

bool check_disk(char *disk_name)
{
    int val = access(disk_name, F_OK);

    if (val != -1)
        return true;

    return false;
}

void sync_stream(void)
{
    cin.clear();
    cout.flush();
}

void clear_all(void)
{
    free_inode_vector.clear();
    free_data_block_vector.clear();
    free_filedescriptor_vector.clear();
    file_descriptor_mode_map.clear();
    file_descriptor_map.clear();
    file_to_inode_map.clear();
    inode_to_file_map.clear();
}

void print_user_menu(void)
{
    cout << "Enter 1 : Create file\n";
    cout << "Enter 2 : Open file\n";
    cout << "Enter 3 : Read file\n";
    cout << "Enter 4 : Write file\n";
    cout << "Enter 5 : Append file\n";
    cout << "Enter 6 : Close file\n";
    cout << "Enter 7 : Delete file\n";
    cout << "Enter 8 : List all files\n";
    cout << "Enter 9 : List of Opened files\n";
    cout << "Enter 10 : Unmount Disk\n";
    cout << "Option - ";
}

void print_main_menu()
{
    cout << "Enter 1 : Create disk\n";
    cout << "Enter 2 : Mount disk\n";
    cout << "Enter 3 : Exit\n";
    cout << "Option - ";
}

int create_disk(string disk_name)
{
    int i{0}, j{0}, len{0};

    char buffer[BLOCK_SIZE] = {};
    char disk[100];

    strcpy(disk, disk_name.c_str());

    bool exist = check_disk(disk);

    if (exist)
    {
        return 0;
    }

    diskptr = fopen(disk, "wb");

    while (i < DISK_BLOCKS)
    {
        fwrite(buffer, 1, BLOCK_SIZE, diskptr);
        i++;
    }
    i = 0;

    super_block sb;

    /* Marking all the blocks other than data blocks as used(super_block,mapping,inodes) */
    while (i < sb.starting_index_of_data_blocks)
    {
        sb.datablock_freelist[i] = (bool)1;
        i++;
    }
    i = 0;

    /* Marking all data bloks as free */
    for (i = sb.starting_index_of_data_blocks; i < DISK_BLOCKS;)
    {
        sb.datablock_freelist[i] = (bool)0;
        i++;
    }
    i = 0;

    /* marking all inodes as free */
    while (i < NO_OF_INODES)
    {
        sb.inode_freelist[i] = (bool)0;
        i++;
    }
    i = 0;

    while (i < NO_OF_INODES)
    {
        /* setting all pointers of inode as free */
        while (j < 12)
        {
            inode_arr[i].pointer[j] = -1;
            j++;
        }
        i++;
    }

    /* storing superblock into starting of the file */
    fseek(diskptr, 0, 0);

    len = sizeof(class super_block);
    char sb_buff[len] = {};

    memcpy(sb_buff, &sb, len);
    fwrite(sb_buff, (size_t)1UL, len, diskptr);

    /* storing file_inode_mapping after super block */
    int skip_count = sb.no_of_blocks_used_by_superblock;
    fseek(diskptr, skip_count * BLOCK_SIZE, 0);

    len = sizeof(file_inode_mapping_arr);
    char dir_buff[len] = {};

    memcpy(dir_buff, file_inode_mapping_arr, len);
    fwrite(dir_buff, (size_t)1UL, len, diskptr);

    /* storing inodes after file_inode mapping */
    skip_count = sb.starting_index_of_inodes;
    fseek(diskptr, skip_count * BLOCK_SIZE, 0);

    len = sizeof(inode_arr);
    char inode_buff[len] = {};

    memcpy(inode_buff, inode_arr, len);
    fwrite(inode_buff, (size_t)1UL, len, diskptr);

    fclose(diskptr);

    return 1;
}

bool mount_disk(string name)
{
    int len{0}, skip_count{0}, i{0};
    char buffer[BLOCK_SIZE] = {};
    char disk[100];

    strcpy(disk, name.c_str());

    diskptr = fopen(disk, "rb+");

    if (!diskptr)
    {
        return false;
    }

    /* retrieve super block from virtual disk and store into global struct super_block sb */
    len = sizeof(sb);
    char sb_buff[len] = {};

    fread(sb_buff, (size_t)1UL, len, diskptr);
    memcpy(&sb, sb_buff, len);

    /* retrieve file_inode mapping array from virtual disk and store into global
    struct dir_entry file_inode_mapping_arr[NO_OF_INODES] */
    skip_count = sb.no_of_blocks_used_by_superblock;
    fseek(diskptr, skip_count * BLOCK_SIZE, SEEK_SET);

    len = sizeof(file_inode_mapping_arr);
    char dir_buff[len] = {};

    fread(dir_buff, (size_t)1UL, len, diskptr);
    memcpy(file_inode_mapping_arr, dir_buff, len);

    /* retrieve Inode block from virtual disk and store into global struct inode 
    inode_arr[NO_OF_INODES] */
    skip_count = sb.starting_index_of_inodes;
    fseek(diskptr, skip_count * BLOCK_SIZE, SEEK_SET);

    len = sizeof(inode_arr);
    char inode_buff[len] = {};

    fread(inode_buff, (size_t)1UL, len, diskptr);
    memcpy(inode_arr, inode_buff, len);

    /* storing all filenames into map */
    i = NO_OF_INODES - 1;

    for (; i > -1;)
    {
        bool if_free{sb.inode_freelist[i]};
        if (if_free)
        {
            string f_name = file_inode_mapping_arr[i].file_name;
            ll inode_num = file_inode_mapping_arr[i].inode_num;

            file_to_inode_map[f_name] = inode_num;
            inode_to_file_map[inode_num] = f_name;
        }
        else if (if_free == false)
        {
            ll inode = i;
            free_inode_vector.push_back(inode);
        }
        i--;
    }

    /* maintain free data block vector */
    i = DISK_BLOCKS - 1;
    int val_check{int(sb.starting_index_of_data_blocks)};

    while (i >= val_check)
    {
        bool if_free{sb.datablock_freelist[i]};
        if (!if_free)
        {
            ll data_block = i;
            free_data_block_vector.push_back(data_block);
        }
        i--;
    }

    /* maintain free filedescriptor vector */
    i = NO_OF_FILE_DESCRIPTORS - 1;

    while (i > -1)
    {
        ll fd = i;
        free_filedescriptor_vector.push_back(fd);
        i--;
    }

    active = true;
    return true;
}

bool unmount_disk(void)
{
    // ll i{0}, len{0};
    // long unsigned int j{0};

    // bool disk_mounted{active == true};
    // if (disk_mounted == false)
    //     return false;

    // /* storing super block into begining of virtual disk */
    // i = DISK_BLOCKS - 1;
    // ll index{sb.starting_index_of_data_blocks};

    // while (i >= index)
    //     sb.datablock_freelist[i--] = bool(1);

    // i = 0;

    // /* updating free data block in super block */
    // size_t free_datablocks = free_data_block_vector.size();

    // for (; j < free_datablocks;)
    // {
    //     ll index = free_data_block_vector[j++];
    //     sb.datablock_freelist[index] = bool(0);
    // }
    // j = 0;

    // /* Initializing inode free list to true */
    // while (i < NO_OF_INODES)
    //     sb.inode_freelist[i++] = bool(1);

    // /* Making those inode nos which are free to false */
    // size_t free_inodes = free_inode_vector.size();
    // for (; j < free_inodes;)
    // {
    //     ll index = free_inode_vector[j++];
    //     sb.inode_freelist[index] = bool(0);
    // }
    // j = 0;

    // /* storing super block structure in starting of virtual disk */
    // fseek(diskptr, 0, 0);

    // len = sizeof(class super_block);
    // char sb_buff[len] = {};

    // memcpy(sb_buff, &sb, len);
    // fwrite(sb_buff, (size_t)1UL, len, diskptr);

    // /* storing file_inode mapping after super block into virtual disk */
    // int skip_count = sb.no_of_blocks_used_by_superblock;
    // fseek(diskptr, skip_count * BLOCK_SIZE, 0);

    // len = sizeof(file_inode_mapping_arr);
    // char dir_buff[len] = {};

    // memcpy(dir_buff, file_inode_mapping_arr, len);
    // fwrite(dir_buff, (size_t)1UL, len, diskptr);

    // /* storing inodes after file_inode mapping into virtual disk */
    // skip_count = sb.starting_index_of_inodes;
    // fseek(diskptr, skip_count * BLOCK_SIZE, 0);

    // len = sizeof(inode_arr);
    // char inode_buff[len] = {};

    // memcpy(inode_buff, inode_arr, len);
    // fwrite(inode_buff, (size_t)1UL, len, diskptr);

    // fclose(diskptr);

    //clear all in-memory data structures
    // clear_all();

    active = false;

    return true;
}

bool create_file(string file_name)
{
    char file[100];
    strcpy(file, file_name.c_str());

    bool file_exist{file_to_inode_map.find(file) != file_to_inode_map.end()};
    if (file_exist == true)
    {
        cout << "Error : File -> " << file << " already present..\n";
        return false;
    }

    size_t avail_inode{free_inode_vector.size()};
    size_t avail_data_block{free_data_block_vector.size()};

    //check if inode are available
    bool inode_exhaust{avail_inode < 1};
    if (inode_exhaust)
    {
        cout << "Error : Inodes exhausted.\n";
        return false;
    }

    //check if datablock are available
    bool data_block_exhaust{avail_data_block < 1};
    if (data_block_exhaust)
    {
        cout << "Error : Datablocks exhausted.\n";
        return false;
    }

    //get next free inode and datablock number
    ll next_avl_inode = free_inode_vector[avail_inode - 1];
    ll next_avl_datablock = free_data_block_vector[avail_data_block - 1];

    free_inode_vector.pop_back();
    free_data_block_vector.pop_back();

    //assigned one data block to this inode
    inode_arr[next_avl_inode].pointer[SEEK_SET] = next_avl_datablock;
    inode_arr[next_avl_inode].filesize = SEEK_SET;

    file_to_inode_map[file] = free_inode_vector[avail_inode - 1];
    inode_to_file_map[next_avl_inode] = file;

    file_inode_mapping_arr[next_avl_inode].inode_num = free_inode_vector[avail_inode - 1];
    strcpy(file_inode_mapping_arr[next_avl_inode].file_name, file);

    return true;
}

ll block_read(ll block, char *buf)
{
    if (active == false)
    {
        cout << "Error : Disk is not active.\n"
             << endl;
        return -1;
    }

    bool index_less{block < 0};
    bool index_more{block >= DISK_BLOCKS};

    if (index_less == true || index_more == true)
    {
        cout << "Error: Block index out of bounds.\n"
             << endl;
        return -1;
    }

    int check = fseek(diskptr, block * BLOCK_SIZE, SEEK_SET);
    if (check < 0)
    {
        cout << "Error : Failed to lseek the block.\n"
             << endl;
        return -1;
    }

    check = fread(buf, sizeof(char), BLOCK_SIZE, diskptr);
    if (check < 0)
    {
        cout << "Error : Failed to read block.\n"
             << endl;
        return -1;
    }

    return 0;
}

ll block_write(int block, char *buf, int size, int start_position)
{

    bool index_less{block < 0};
    bool index_more{block >= DISK_BLOCKS};

    if (index_less == true || index_more == true)
    {
        cout << "Error: Block index out of bounds.\n\n";
        return -1;
    }

    int check = fseek(diskptr, (block * BLOCK_SIZE) + start_position, 0);
    if (check < 0)
    {
        cout << "Error: Failed to lseek.\n\n";
        return -1;
    }

    size_t check1 = fwrite(buf, sizeof(char), size, diskptr);
    if (check1 < 0)
    {
        cout << "Error: Failed to Write.\n\n";
        return -1;
    }

    return 0;
}

ll erase_inode_content(int cur_inode)
{
    int i{0}, j{0};
    ll indirectptr, doubleindirectptr, singleindirectptr;
    ll count{1024};
    //flag to verify if deletion completed or not
    int delete_completed{0};

    //iterate over direct pointers
    while (i < 10)
    {
        bool break_cond{inode_arr[cur_inode].pointer[i] == -1};
        if (break_cond)
        {
            delete_completed = 1;
            break;
        }
        else
        {
            ll free_db = inode_arr[cur_inode].pointer[i];
            free_data_block_vector.push_back(free_db);
            inode_arr[cur_inode].pointer[i] = -1;
        }
        i++;
    }
    i = 0;

    if (delete_completed == 0)
    {
        //reading content of block pointed by indirect pointer
        int index{10};
        indirectptr = inode_arr[cur_inode].pointer[index];

        char blockbuffer[BLOCK_SIZE];
        ll indirect_ptr_array[1024];

        block_read(indirectptr, blockbuffer);
        memcpy(indirect_ptr_array, blockbuffer, sizeof(indirect_ptr_array));

        for (; i < count;)
        {
            bool check{indirect_ptr_array[i] == -1};
            if (check == true)
            {
                delete_completed = 1;
                break;
            }
            else
            {
                ll free_db = indirect_ptr_array[i];
                free_data_block_vector.push_back(free_db);
            }
            i++;
        }
        i = 0;

        inode_arr[cur_inode].pointer[index] = -1;
        free_data_block_vector.push_back(indirectptr);
    }
    i = 0;

    if (delete_completed == 0)
    {
        int index{11};
        doubleindirectptr = inode_arr[cur_inode].pointer[index];

        char blockbuffer[BLOCK_SIZE];
        ll doubleindirect_ptr_array[count];

        block_read(doubleindirectptr, blockbuffer);
        memcpy(doubleindirect_ptr_array, blockbuffer, sizeof(doubleindirect_ptr_array));

        for (; i < count;)
        {
            bool check{doubleindirect_ptr_array[i] == -1};
            if (check == true)
            {
                delete_completed = 1;
                break;
            }
            else
            {
                singleindirectptr = doubleindirect_ptr_array[i];

                char blockbuffer1[BLOCK_SIZE];
                ll indirect_ptr_array[count];

                block_read(singleindirectptr, blockbuffer1);
                memcpy(indirect_ptr_array, blockbuffer1, sizeof(indirect_ptr_array));

                for (; j < count;)
                {
                    bool check{indirect_ptr_array[j] == -1};
                    if (check)
                    {
                        delete_completed = 1;
                        break;
                    }
                    else
                    {
                        ll free_db = indirect_ptr_array[i];
                        free_data_block_vector.push_back(free_db);
                    }
                    j++;
                }

                free_data_block_vector.push_back(singleindirectptr);
            }
            i++;
        }
        i = 0;

        free_data_block_vector.push_back(doubleindirectptr);
    }
    i = 0;

    //Resetting inode structure with default values.
    while (i < 12)
        inode_arr[cur_inode].pointer[i++] = -1;

    inode_arr[cur_inode].filesize = 0;

    return 0;
}

bool delete_file(string file_name)
{
    int i{0};
    char file[100];
    strcpy(file, file_name.c_str());

    //check if file exist or not
    bool file_exist{file_to_inode_map.find(file) != file_to_inode_map.end()};
    if (file_exist == false)
    {
        cout << "Error : File - " << file << " not found" << endl;
        return false;
    }

    //getting inode of file
    // ll cur_inode = file_to_inode_map[file];
    // while (i < 32)
    // {
    //     bool check_1{file_descriptor_map.find(i) != file_descriptor_map.end()};
    //     bool check_2{file_descriptor_map[i].first == file_to_inode_map[file]};

    //     if (check_1 && check_2)
    //     {
    //         cout << "Error : File - " << file << " is currently Open. Can not be deleted. \n\n";
    //         return false;
    //     }
    //     i++;
    // }

    erase_inode_content(file_to_inode_map[file]);
    free_inode_vector.push_back(file_to_inode_map[file]);

    string empty_str = "";
    ll index = file_to_inode_map[file];
    strcpy(file_inode_mapping_arr[index].file_name, empty_str.c_str());

    file_inode_mapping_arr[file_to_inode_map[file]].inode_num = -1;

    inode_to_file_map.erase(file_to_inode_map[file]);
    file_to_inode_map.erase(file);

    return true;
}

ll open_file(string file_name)
{
    char file[100];
    strcpy(file, file_name.c_str());

    int erorr_code{-1}, file_mode{-1}, i{0};

    bool file_exists{file_to_inode_map.find(file) != file_to_inode_map.end()};
    if (file_exists == false)
    {
        cout << "Error : File " << file << " not found in the disk..\n";
        return erorr_code;
    }

    bool fd_not_avail{free_filedescriptor_vector.empty()};
    if (fd_not_avail)
    {
        cout << "Error : File Descriptor is not available..\n";
        return erorr_code;
    }

    /* asking for mode of file  */
    while (file_mode == -1)
    {
        int input;

        cout << "Choose the mode you want the File : " << file << " to open in..\n"
             << "0 : Read mode\n"
             << "1 : Write mode\n"
             << "2 : Append mode\n";
        cout << "Option - ";
        cin >> input;

        bool invalid_input(input > 2 || input < 0);
        if (invalid_input)
        {
            cout << "Enter Valid Choice {0,1,2}\n";
            continue;
        }
        else
        {
            file_mode = input;
        }
    }

    /* checking if file is already open in write or append mode. */
    i = 0;
    bool write_append(file_mode == 1 || file_mode == 2);
    // if (write_append)
    // {
    //     while (i < 32)
    //     {
    //         int mode_i = file_descriptor_mode_map[i];

    //         bool check_1{file_descriptor_map.find(i) != file_descriptor_map.end()};
    //         bool check_2{file_descriptor_map[i].first == file_to_inode_map[file]};
    //         bool check_3{mode_i == 1 || mode_i == 2};

    //         if (check_1 && check_2 && check_3)
    //         {
    //             cout << "File " << file << " is already Opened with File Descriptor : " << i << "\n";
    //             return erorr_code;
    //         }
    //         i++;
    //     }
    // }

    long unsigned int count_fd{free_filedescriptor_vector.size()};
    ll fd = free_filedescriptor_vector[count_fd - 1];

    free_filedescriptor_vector.pop_back();

    file_descriptor_map[fd].first = file_to_inode_map[file];
    file_descriptor_map[fd].second = SEEK_SET;

    file_descriptor_mode_map[fd] = file_mode;

    openfile_count += 1;

    string mode = "";

    if (file_mode == 0)
        mode = "READ";
    else if (file_mode == 1)
        mode = "WRITE";
    else if (file_mode == 2)
        mode = "APPEND";

    cout << "File: " << file << " is now opened in " << mode << " mode..\n"
         << "File Desciptor -> " << fd << "\n\n";

    return fd;
}

ll write_helper(int fd, char *buff, int len)
{
    ll file_inode = file_descriptor_map[fd].first;
    ll cur_pos = file_descriptor_map[fd].second;
    ll filled_data_block = cur_pos / BLOCK_SIZE;
    ll count{10}, cnt{1024};

    /* if last Data block is partially filled */
    bool cond{cur_pos % BLOCK_SIZE == 0};
    if (cond == false)
    {
        /* Write file into direct pointed block */
        if (filled_data_block < count)
        {
            ll data_block_to_write = inode_arr[file_inode].pointer[filled_data_block];
            ll index = cur_pos % BLOCK_SIZE;
            block_write(data_block_to_write, buff, len, index);

            inode_arr[file_inode].filesize += len;
            file_descriptor_map[fd].second += len; //updating the cur pos in the file
        }
        /* Write file into single Indirect block */
        else if (filled_data_block < (count + cnt))
        {
            ll desc_ind = file_descriptor_map[fd].first;
            ll block = inode_arr[desc_ind].pointer[count];
            ll block_pointers[cnt]; //Contains the array of data block pointers.
            char read_buf[BLOCK_SIZE];

            block_read(block, read_buf); //reading the block into read_buf
            memcpy(block_pointers, read_buf, 4096UL);

            ll diff = filled_data_block - count;
            int data_block_to_write = block_pointers[diff];
            ll index = cur_pos % BLOCK_SIZE;

            block_write(data_block_to_write, buff, len, index);

            inode_arr[file_inode].filesize += len;
            file_descriptor_map[fd].second += len; //updating the cur pos in the file
        }
        /* Write file into double Indirect block */
        else
        {
            ll desc_ind = file_descriptor_map[fd].first;
            ll block = inode_arr[desc_ind].pointer[count + 1];
            ll block_pointers[cnt]; //Contains the array of data block pointers.
            char read_buf[BLOCK_SIZE];

            block_read(block, read_buf); //reading the block into read_buf
            memcpy(block_pointers, read_buf, 4096UL);

            ll diff = filled_data_block - (count + cnt);
            ll block2 = block_pointers[diff / cnt];
            ll block_pointers2[cnt]; //Contains the array of data block pointers.

            block_read(block2, read_buf); //reading the block2 into read_buf
            memcpy(block_pointers2, read_buf, 4096UL);

            int data_block_to_write = block_pointers[diff / cnt];
            ll index = cur_pos % BLOCK_SIZE;

            block_write(data_block_to_write, buff, len, index);

            inode_arr[file_inode].filesize += len;
            file_descriptor_map[fd].second += len; //updating the cur pos in the file
        }
    }
    /* if last data block is fully filled */
    else
    {
        /* if filesize = 0 means file is empty then start writting into file from first direct block. */
        ll desc_ind = file_descriptor_map[fd].first;
        bool check{inode_arr[desc_ind].filesize == 0};
        if (check)
        {
            block_write(inode_arr[file_inode].pointer[0], buff, len, 0);

            int res = inode_arr[file_inode].filesize + len;
            inode_arr[file_inode].filesize = res;

            res = file_descriptor_map[fd].second + len;
            file_descriptor_map[fd].second = len;
        }
        /* if filesize != 0 then */
        else
        {
            bool pass{cur_pos == 0};
            if (pass)
            {
                /* flush all data blocks and start writting into file from first block */
                bool check{file_descriptor_map[fd].second == 0};
                if (check == true)
                {
                    erase_inode_content(file_inode);
                    //check if datablock are available
                    size_t fb_size = free_data_block_vector.size();
                    if (fb_size == 0)
                    {
                        cout << "Error : No more Datablock available to write.\n\n";
                        return -1;
                    }

                    ll next_avl_datablock = free_data_block_vector[fb_size - 1];

                    free_data_block_vector.pop_back();
                    inode_arr[file_inode].pointer[SEEK_SET] = next_avl_datablock;
                }
                block_write(inode_arr[file_inode].pointer[SEEK_SET], buff, len, SEEK_SET);

                int res = inode_arr[file_inode].filesize + len;
                inode_arr[file_inode].filesize = res;

                res = file_descriptor_map[fd].second + len;
                file_descriptor_map[fd].second = res;
            }
            else
            {
                if (filled_data_block < count)
                {
                    //check if datablock are available
                    size_t fb_size = free_data_block_vector.size();
                    if (fb_size == 0)
                    {
                        cout << "Error : No more DataBlock available.\n\n";
                        return -1;
                    }
                    ll data_block_to_write = free_data_block_vector[fb_size - 1];
                    free_data_block_vector.pop_back();

                    inode_arr[file_inode].pointer[filled_data_block] = data_block_to_write;

                    ll ind_1 = cur_pos % BLOCK_SIZE;
                    block_write(data_block_to_write, buff, len, ind_1);

                    int res = inode_arr[file_inode].filesize + len;
                    inode_arr[file_inode].filesize = res;

                    res = file_descriptor_map[fd].second + len;
                    file_descriptor_map[fd].second = res; //updating the cur pos in the file
                }
                else if (filled_data_block < (count + cnt))
                {
                    if (filled_data_block == count) //i.e all direct DB pointer 0-9 is full then need to allocate a DB to pointer[10] & store 1024 DB in this DB
                    {
                        ll k{0};
                        ll block_pointers[cnt];

                        while (k < cnt)
                            block_pointers[k++] = -1;

                        /* to store block_pointers[1024] into db_for_single_indirect */
                        //check if datablock are available
                        size_t fb_size = free_data_block_vector.size();
                        if (fb_size == 0)
                        {
                            cout << "Error : No more Datablocks available.\n\n";
                            return -1;
                        }

                        ll data_block_single_indirect = free_data_block_vector[fb_size - 1];
                        free_data_block_vector.pop_back();

                        inode_arr[file_inode].pointer[count] = data_block_single_indirect;

                        char temp_buf[BLOCK_SIZE];
                        memcpy(temp_buf, block_pointers, BLOCK_SIZE);

                        block_write(data_block_single_indirect, temp_buf, BLOCK_SIZE, SEEK_SET);
                    }

                    ll block = inode_arr[file_inode].pointer[count];
                    ll block_pointers[cnt]; //Contains the array of data block pointers.

                    char read_buf[BLOCK_SIZE];
                    block_read(block, read_buf); //reading the single indirect block into read_buf

                    memcpy(block_pointers, read_buf, 4096UL); //moving the data into blockPointer

                    //check if datablock are available
                    size_t fb_size = free_data_block_vector.size();
                    if (fb_size == 0)
                    {
                        cout << "Error : No more Datablocks available" << endl;
                        return -1;
                    }

                    ll data_block_to_write = free_data_block_vector[fb_size - 1];
                    free_data_block_vector.pop_back();

                    //store back the blockPointer to disk
                    block_pointers[filled_data_block - count] = data_block_to_write;
                    char temp_buf[BLOCK_SIZE];

                    memcpy(temp_buf, block_pointers, BLOCK_SIZE);
                    block_write(block, temp_buf, BLOCK_SIZE, SEEK_SET);

                    //write data into DB
                    block_write(data_block_to_write, buff, len, SEEK_SET);

                    inode_arr[file_inode].filesize += len;
                    file_descriptor_map[fd].second += len; //updating the cur pos in the file
                }
                else
                {
                    bool chck{filled_data_block == (count + cnt)};
                    if (chck == true)
                    {
                        ll i{0};
                        ll block_pointers[cnt];

                        while (i < cnt)
                            block_pointers[i++] = -1;

                        //check if datablock are available
                        size_t fb_size = free_data_block_vector.size();
                        if (fb_size == 0)
                        {
                            cout << "Error : No more Datablock available.\n\n";
                            return -1;
                        }

                        ll data_block_double_indirect = free_data_block_vector[fb_size - 1]; //to store block_pointers[1024] into db_for_double_indirect
                        free_data_block_vector.pop_back();

                        inode_arr[file_inode].pointer[count + 1] = data_block_double_indirect;

                        char temp_buf[BLOCK_SIZE];
                        memcpy(temp_buf, block_pointers, BLOCK_SIZE);

                        block_write(data_block_double_indirect, temp_buf, BLOCK_SIZE, SEEK_SET);
                    }
                    if ((filled_data_block - (count + cnt)) % cnt == 0) //i.e if filled_data_block is multiple of 1024 means need new DB to be assigned
                    {
                        ll block = inode_arr[file_inode].pointer[count + 1];
                        ll block_pointers[cnt]; //Contains the array of data block pointers.

                        char read_buf[BLOCK_SIZE];

                        block_read(block, read_buf); //reading the block into read_buf
                        memcpy(block_pointers, read_buf, 4096UL);

                        ll block_pointers2[cnt];
                        ll i{0};

                        while (i < cnt)
                            block_pointers2[i++] = -1;

                        //check if datablock are available
                        size_t fb_size = free_data_block_vector.size();
                        if (fb_size == 0)
                        {
                            cout << "Error : No more Datablocks available.\n\n";
                            return -1;
                        }

                        ll data_block_double_indirect2 = free_data_block_vector[fb_size - 1]; //to store block_pointers[1024] into db_for_double_indirect
                        free_data_block_vector.pop_back();

                        ll ind = (filled_data_block - (count + cnt)) / cnt;
                        block_pointers[ind] = data_block_double_indirect2;

                        char temp_buf[BLOCK_SIZE];

                        memcpy(temp_buf, block_pointers2, BLOCK_SIZE);
                        block_write(data_block_double_indirect2, temp_buf, BLOCK_SIZE, SEEK_SET);

                        memcpy(temp_buf, block_pointers, BLOCK_SIZE);
                        block_write(block, temp_buf, BLOCK_SIZE, SEEK_SET);
                    }

                    ll block = inode_arr[file_inode].pointer[count + 1];
                    ll block_pointers[cnt]; //Contains the array of data block pointers.

                    char read_buf[BLOCK_SIZE];

                    block_read(block, read_buf); //reading the block into read_buf
                    memcpy(block_pointers, read_buf, BLOCK_SIZE);

                    ll ind = (filled_data_block - (count + cnt)) / cnt;
                    ll block2 = block_pointers[ind];
                    ll block_pointers2[cnt]; //Contains the array of data block pointers.

                    char read_buf2[BLOCK_SIZE];

                    block_read(block2, read_buf2); //reading the block2 into read_buf2
                    memcpy(block_pointers2, read_buf2, BLOCK_SIZE);

                    //check if datablock are available
                    size_t fb_size = free_data_block_vector.size();
                    if (fb_size == 0)
                    {
                        cout << "Error : No more Datablocks available" << endl;
                        return -1;
                    }

                    ll data_block_to_write = free_data_block_vector[fb_size - 1]; //to store block_pointers[1024] into db_for_double_indirect
                    free_data_block_vector.pop_back();

                    ll ind1 = (filled_data_block - (count + cnt)) % cnt;
                    block_pointers2[ind1] = data_block_to_write;

                    block_write(data_block_to_write, buff, len, SEEK_SET); //writing data into db_to_write DB

                    //now restore block_pointers2 back to the block2
                    char temp_buf[BLOCK_SIZE];

                    memcpy(temp_buf, block_pointers2, BLOCK_SIZE);
                    block_write(block2, temp_buf, BLOCK_SIZE, SEEK_SET);

                    //updating the filesize
                    inode_arr[file_inode].filesize += len;
                    file_descriptor_map[fd].second += len; //updating the cur pos in the file
                }
            }
        }
    }
    return 0;
}

ll write_file(ll fd, string mode)
{
    int i{0}, len{-1};

    //check if file exist or not
    bool file_exist{file_descriptor_map.find(fd) != file_descriptor_map.end()};
    if (file_exist == false)
    {
        cout << "Error : File descriptor " << fd << " doesn't exist.\n\n";
        return -1;
    }

    //getting inode of file
    ll cur_inode = file_descriptor_map[fd].first;
    if (mode == "write")
    {
        /* Write  */
        bool check{file_descriptor_mode_map[fd] != 1};
        if (check == true)
        {
            cout << "Error : File descriptor " << fd << " is not opened in WRITE mode.\n\n";
            return -1;
        }

        // Make All read pointers at start of file.[ set second elem of pair to 0 ]
        for (; i < 32;)
        {
            bool check_1{file_descriptor_map.find(i) != file_descriptor_map.end()};
            bool check_2{file_descriptor_map[i].first == cur_inode};
            bool check_3{file_descriptor_mode_map[i] == 0};

            if (check_1 && check_2 && check_3)
                file_descriptor_map[i].second = 0;

            i++;
        }
    }
    else
    {
        /* Append */
        bool check{file_descriptor_mode_map[fd] != 2};
        if (check == true)
        {
            cout << "Error : File descriptor " << fd << " is not opened in append mode.\n\n";
            return -1;
        }
        file_descriptor_map[fd].second = inode_arr[cur_inode].filesize;
    }

    char message[10000];
    // string input;

    cout << "Enter content to write into file : \n>>> ";

    // while (true)
    // {
    //     string temp;
    //     getline(cin, temp);
    //     if (temp == "bye")
    //     {
    //         input += '\0';
    //         break;
    //     }
    //     input += temp;
    // }

    cin.getline(message, 10000);
    cin.getline(message, 10000);

    string input(message);

    //checking how many in last DB have space remaing to store data
    ll diff = (file_descriptor_map[fd].second) % BLOCK_SIZE;
    unsigned int remaining = BLOCK_SIZE - diff;

    //if remaining empty size in last data block is more than buffer size then write directly in last data block
    bool check_size{remaining >= input.size()};
    if (check_size == true)
    {
        len = input.size();
        char buff[len + 1] = {};

        memcpy(buff, input.c_str(), len);

        buff[len] = '\0';

        write_helper(fd, buff, input.size());
    }
    else
    {
        //1st write remaining size in last written data block
        len = remaining;
        char buff[len + 1] = {};
        memcpy(buff, input.c_str(), len);

        buff[len] = '\0';
        write_helper(fd, buff, remaining);

        input = input.substr(len);

        //write block by block till last block
        ll len__ = input.size();
        ll remaining_block_count = len__ / BLOCK_SIZE;

        while (remaining_block_count > 0)
        {
            len = BLOCK_SIZE;
            char buff[len + 1] = {};

            memcpy(buff, input.c_str(), len);
            buff[len] = '\0';

            input = input.substr(len);

            int check_write = write_helper(fd, buff, len);
            if (check_write == -1)
            {
                cout << "Not Enough space..\n\n";
                return -1;
            }
            remaining_block_count -= 1;
        }

        //write last partial block
        size_t len_inp = input.size();
        ll remaining_size = len_inp % BLOCK_SIZE;

        len = remaining_size;
        char buff1[len + 1] = {};

        memcpy(buff1, input.c_str(), len);
        buff1[len] = '\0';

        int check_write = write_helper(fd, buff1, len);
        if (check_write == -1)
        {
            cout << "Not Enough space.. \n\n";
            return -1;
        }
    }

    cout << "Content was written onto the file successfully.\n\n";
    return 0;
}

ll read_file(ll fd)
{
    bool partial_read{false};

    ll file_descriptor{fd};
    ll cur_inode = file_descriptor_map[file_descriptor].first;
    ll fs = file_descriptor_map[file_descriptor].second;

    bool check{file_descriptor_map.find(file_descriptor) == file_descriptor_map.end()};
    if (check)
    {
        cout << "Error : File is not opened yet.\n\n";
        return -1;
    }

    check = (file_descriptor_mode_map[file_descriptor] != 0);
    if (check)
    {
        cout << "Error : File with descriptor - " << file_descriptor << " is not opened in read mode.\n\n";
        return -1;
    }

    struct inode in = inode_arr[cur_inode];
    ll filesize = in.filesize;

    float blocks = (float)inode_arr[cur_inode].filesize;
    ll noOfBlocks = ceil(blocks / BLOCK_SIZE);
    ll tot_block = noOfBlocks; // tot_block = number of blocks to read and noOfBlocks = blocks left to read

    char read_buf[BLOCK_SIZE];
    char *buf = new char[filesize];
    char *initial_buf_pos = buf;

    ll bytes_read{0}, i{0}, count{10};

    while (i < count)
    {
        if (noOfBlocks == 0)
            break;

        ll block_no = in.pointer[i];

        block_read(block_no, read_buf);

        ll diff_block = tot_block - noOfBlocks;
        bool check_1{diff_block < fs / BLOCK_SIZE};

        if (check_1 == false && (noOfBlocks >= 2))
        {
            if (!partial_read)
            {
                ll fs_div = fs % BLOCK_SIZE;
                ll fs_diff = BLOCK_SIZE - fs_div;

                memcpy(buf, read_buf + fs_div, (BLOCK_SIZE - fs_div));

                buf = buf + fs_diff;
                bytes_read += fs_diff;

                partial_read = true;
            }
            else
            {
                memcpy(buf, read_buf, BLOCK_SIZE);

                buf = buf + BLOCK_SIZE;
                bytes_read += BLOCK_SIZE;
            }
        }
        noOfBlocks--;
        i++;
    }

    if (noOfBlocks != 0) //Just to check any single indirect pointers are used or not
    {
        ll i{0}, count{10}, cnt{1024};
        ll block = inode_arr[cur_inode].pointer[count];
        ll blockPointers[cnt]; //Contains the array of data block pointers.

        block_read(block, read_buf);
        memcpy(blockPointers, read_buf, 4096UL);

        while (noOfBlocks && i < cnt)
        {
            block_read(blockPointers[i], read_buf);

            ll diff_block = tot_block - noOfBlocks;
            bool check_1{diff_block < fs / BLOCK_SIZE};

            if (check_1 == false && noOfBlocks > 1)
            {
                if (!partial_read)
                {
                    ll fs_div = fs % BLOCK_SIZE;
                    ll fs_diff = BLOCK_SIZE - fs_div;

                    memcpy(buf, read_buf + fs_div, (BLOCK_SIZE - fs_div));

                    buf = buf + fs_diff;
                    bytes_read += fs_diff;

                    partial_read = true;
                }
                else
                {
                    memcpy(buf, read_buf, BLOCK_SIZE);

                    buf = buf + BLOCK_SIZE;
                    bytes_read += BLOCK_SIZE;
                }
            }
            noOfBlocks--;
            i++;
        }
    }

    if (noOfBlocks != 0) //Indirect pointers done check for Double Indirect
    {
        ll i{0}, count{10}, cnt{1024};
        ll block = inode_arr[cur_inode].pointer[count + 1];
        ll indirectPointers[cnt]; //Contains array of indirect pointers

        block_read(block, read_buf);
        memcpy(indirectPointers, read_buf, 4096UL);

        while (noOfBlocks && i < cnt)
        {
            ll blockPointers[cnt], j{0};

            block_read(indirectPointers[i], read_buf);
            memcpy(blockPointers, read_buf, 4096UL);

            while (noOfBlocks && j < cnt)
            {
                block_read(blockPointers[j], read_buf);

                ll diff_block = tot_block - noOfBlocks;
                bool check_1{diff_block < fs / BLOCK_SIZE};

                if (check_1 == false && noOfBlocks > 1)
                {
                    if (!partial_read)
                    {
                        ll fs_div = fs % BLOCK_SIZE;
                        ll fs_diff = BLOCK_SIZE - fs_div;

                        memcpy(buf, read_buf + fs_div, (BLOCK_SIZE - fs_div));

                        buf = buf + fs_diff;
                        bytes_read += fs_diff;

                        partial_read = true;
                    }
                    else
                    {
                        memcpy(buf, read_buf, BLOCK_SIZE);

                        buf = buf + BLOCK_SIZE;
                        bytes_read += BLOCK_SIZE;
                    }
                }
                noOfBlocks--;
                j++;
            }
            i++;
        }
    }

    ll block_cnt = tot_block - fs / BLOCK_SIZE;
    bool gt_1{block_cnt > 1};
    bool eq_1{block_cnt == 1};
    if (gt_1)
    {
        ll f_size = inode_arr[cur_inode].filesize;
        ll f_div = f_size % BLOCK_SIZE;

        memcpy(buf, read_buf, f_div);
        bytes_read += f_div;
    }
    else if (eq_1)
    {
        memcpy(buf, read_buf + (fs % BLOCK_SIZE), (inode_arr[cur_inode].filesize) % BLOCK_SIZE - fs % BLOCK_SIZE);
        bytes_read += (inode_arr[cur_inode].filesize) % BLOCK_SIZE - fs % BLOCK_SIZE;
    }

    initial_buf_pos[bytes_read] = '\0';
    cout.flush();
    cout << "Inside File : \n\n"
         << initial_buf_pos << "\n\n";
    cout.flush();
    cout << "File was read successfully.\n\n";
    return 1;
}

bool close_file(ll file_descriptor)
{
    bool file_not_open{file_descriptor_map.find(file_descriptor) == file_descriptor_map.end()};
    if (file_not_open == true)
        return false;

    auto i = file_descriptor_map.find(file_descriptor);
    file_descriptor_map.erase(i);

    auto j = file_descriptor_mode_map.find(file_descriptor);
    file_descriptor_mode_map.erase(j);

    free_filedescriptor_vector.push_back(file_descriptor);

    openfile_count -= 1;

    return true;
}

ll list_files(void)
{
    int counter{1};
    cout << "All files present in the Current Disk : {inode -> file_name}\n";

    for (auto i : file_to_inode_map)
    {
        cout << counter++ << ".) " << i.second << " -> " << i.first << "\n";
    }

    if (file_to_inode_map.empty())
    {
        cout << "NO files present in the disk..\n\n";
    }
    cout << endl;

    return 0;
}

ll list_open_files(void)
{
    ll fd{-1}, inode{-1};
    int file_mode{-1}, count{1};
    string name = "";
    vector<string> files_read, files_write, files_append;

    for (auto i : file_descriptor_map)
    {
        fd = i.first;
        inode = i.second.first;

        file_mode = file_descriptor_mode_map[fd];
        name = inode_to_file_map[inode];

        if (file_mode == 0)
        {
            if (find(files_read.begin(), files_read.end(), name) == files_read.end())
                files_read.push_back(name);
        }
        else if (file_mode == 1)
        {
            if (find(files_write.begin(), files_write.end(), name) == files_write.end())
                files_write.push_back(name);
        }
        else if (file_mode == 2)
        {
            if (find(files_append.begin(), files_append.end(), name) == files_append.end())
                files_append.push_back(name);
        }
    }

    if (files_read.empty() == false)
    {
        cout << "List of files in READ MODE : \n";
        for (auto i : files_read)
        {
            cout << count++ << ".) " << i << "\n";
        }
        cout << endl;
    }
    count = 1;

    if (files_write.empty() == false)
    {
        cout << "List of files in WRITE MODE : \n";
        for (auto i : files_write)
        {
            cout << count++ << ".) " << i << "\n";
        }
        cout << endl;
    }
    count = 1;

    if (files_append.empty() == false)
    {
        cout << "List of files in APPEND MODE : \n";
        for (auto i : files_append)
        {
            cout << count++ << ".) " << i << "\n";
        }
        cout << endl;
    }

    if (files_read.empty() && files_write.empty() && files_append.empty())
    {
        cout << "No File is Opened yet in any mode..\n\n";
    }

    return 0;
}

bool handle_request(int command)
{
    int fd{-1};

    //CREATE_FILE
    if (command == 1)
    {
        cout << "Enter the name of file you wish to create: ";
        cin >> file_name;

        bool file = create_file(file_name);

        if (file)
            cout << file_name << " has been created successfully..\n\n";
        else
            cout << "Error: File couldn't be created.\n\n";

        return false;
    }

    //OPEN_FILE
    else if (command == 2)
    {
        cout << "Enter the name of file you wish to open: ";
        cin >> file_name;

        ll open = open_file(file_name);

        if (open == -1)
            cout << "Error: File couldn't be opened..\n\n";
        else
            fd = open;

        return false;
    }

    //READ_FILE
    else if (command == 3)
    {
        cout << "Enter the file_descriptor of the file you wish to read: ";
        cin >> fd;
        ll read = read_file(fd);
        sync_stream();
        return false;
    }

    //WRITE_FILE
    else if (command == 4)
    {
        cout << "Enter the file_descriptor of the file you wish to write data to: ";
        cin >> fd;
        int write = write_file(fd, "write");
        sync_stream();
        return false;
    }

    //APPEND_FILE
    else if (command == 5)
    {
        cout << "Enter the file_descriptor of the file you wish to append data to: ";
        cin >> fd;
        int write = write_file(fd, "append");
        sync_stream();
        return false;
    }

    //CLOSE_FILE
    else if (command == 6)
    {
        cout << "Enter the file_descriptor of the file you wish to close: ";
        cin >> fd;

        bool close = close_file(ll(fd));

        if (close)
            cout << "File has been closed successfully. \n\n";
        else
            cout << "Error : File which is not open cann't be closed.\n\n";

        return false;
    }

    //DELETE_FILE
    else if (command == 7)
    {
        cout << "Enter the name of file you wish to delete: ";
        cin >> file_name;

        bool del = delete_file(file_name);

        if (del)
            cout << "File : " << file_name << " has been deleted successfully..\n\n";
        else
            cout << "Error : File couldn't be Deleted. \n\n";

        return false;
    }

    //LIST_FILES
    else if (command == 8)
    {
        list_files();
        return false;
    }

    //LIST_OPENED_FILES
    else if (command == 9)
    {
        list_open_files();
        return false;
    }

    //UNMOUNT_DISK
    else if (command == 10)
    {
        bool mount = unmount_disk();

        if (mount == true)
            cout << "Disk has been UNMOUNTED..\n\n";
        else
            cout << "Error: Disk has not been mounted..\nTry again (Create->Mount)\n\n";

        return true;
    }

    //INVALID_INPUT
    else
    {
        cout << "Enter valid integer -> {0 - 10}\n\n";
        return false;
    }
}

int main()
{
    int option, command;
    cout << "<<-- Welcome -->>\n\n";
    while (true)
    {
        print_main_menu();

        cin.clear();
        cin >> option;

        if (option == 3)
        {
            cout << "BYE..\n";
            exit(0);
        }
        else if (option == 1)
        {
            cout << "Enter name of the disk to be created : ";
            cin >> disk_name;

            int disk = create_disk(disk_name);

            if (disk == 0)
            {
                cout << "A disk with same name exists..\n<<Retry>>\n";
            }
            else if (disk == 1)
            {
                cout << "Virtual Disk -> " << disk_name << " has been created.. \n\n";
            }
        }
        else if (option == 2)
        {
            cout << "Enter name of the disk you wish to mount : ";
            cin >> disk_name;

            bool disk = mount_disk(disk_name);

            if (disk == true)
            {
                cout << "Virtual Disk -> " << disk_name << " has been mounted.. \n\n";
                while (true)
                {
                    print_user_menu();

                    cin.clear();
                    cin >> command;

                    bool exit = handle_request(command);

                    if (exit == true)
                        break;
                    else
                        continue;
                }
            }
            else
            {
                cout << "Error : Disk couldn't be mounted.\nDisk does not exist..\n\n";
            }
        }
        else
        {
            cout << "Enter valid integer - {1,2,3}\n";
        }
    }

    return 0;
}