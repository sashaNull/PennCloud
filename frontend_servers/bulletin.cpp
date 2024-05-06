#include "./backend_communication.h"
#include "../utils/utils.h"
#include "./bulletin.h"

using namespace std;

int render_bulletin_board(string uids) {
    // parse item uids (uid1, uid2, ....)
    // for each uid, fetch (bulletin/uid owner, bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
    // store as list of BulletinMsg objects
    // render bulletin board
}

int render_my_bulletins(string uids) {
    // parse item uids (uid1, uid2, ....)
    // for each uid, fetch (bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
    // store as list of BulletinMsg objects
    // render as list with edit and delete buttons
    // if click on edit button, /edit-bulletin?title=<title>&msg=<msg>
    // if click on delete button, /delete-bulletin
}

int put_bulletin_item_to_backend(string username, string encoded_owner, string encoded_ts, string encoded_title, string encoded_msg) {
    // cput uid into (bullet-board items)
    // put bulletin/uid owner, bulletin/uid timestamp, bulletin/uid title, bulletin/uid message
    // cput uid into (username_bulletin items)
}

int delete_bulletin_item(string username, string uid) {
    // cput new (bullet-board items)
    // cput new (username_bulletin items)
    // delete bulletin/uid owner, timestamp, title, message

}