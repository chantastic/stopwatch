
#undef rename
#undef fsync
}
ProfileSnapshot profile_snapshot(){std::lock_guard<std::mutex> lock(data_mutex);return stored;}
// NATIVE_INITIALIZER_HERE
}
std::vector<uint8_t> bytes(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write_bytes(const std::string& p,const std::vector<uint8_t>& b){std::ofstream f(p,std::ios::binary);f.write((const char*)b.data(),b.size());}
int main(int argc,char**argv){
 assert(argc==2);char tmp[]="/tmp/native-badge-profile-XXXXXX";root=mkdtemp(tmp);badge::record_path=root+"/conference-manual-v1.bin";badge::temp_path=root+"/conference-manual-v1.tmp";
 auto jpeg=bytes(argv[1]);assert(!jpeg.empty());
 ConferenceProfileStore legacy;assert(legacy.begin());ConferenceProfile old;old.name="Synthetic Native";old.urls[0]="https://github.com/example";assert(legacy.save(old,jpeg.data(),jpeg.size(),true));
 badge::ProfileSnapshot imported;assert(badge::read_record(badge::record_path.c_str(),imported));assert(imported.profile.name==old.name.c_str());assert(imported.avatar&&imported.avatar->size()==25600);assert(memcmp(imported.avatar->data(),legacy.avatarPixels(),51200)==0);
 imported.ready=true;badge::stored=imported;auto next=imported.profile;next.name="Changed Native";next.urls[1]="https://x.com/example_1";std::string error;assert(badge::save_record(next,nullptr,0,false,error));
 {ConferenceProfileStore loaded;assert(loaded.begin());assert(loaded.profile().name==next.name.c_str());assert(loaded.profile().urls[1]==next.urls[1].c_str());assert(loaded.avatarPixels());}
 auto unchanged=bytes(badge::record_path);next.name="Failure must not commit";
 for(bool* failure:{&rename_failed,&sync_failed}){*failure=true;assert(!badge::save_record(next,nullptr,0,false,error));*failure=false;assert(bytes(badge::record_path)==unchanged);assert(badge::stored.profile.name=="Changed Native");}
 for(size_t pos:{size_t(0),size_t(8),size_t(12),size_t(16),size_t(24),size_t(32),size_t(68),unchanged.size()-1}){auto corrupt=unchanged;corrupt[pos]^=0x80;write_bytes(badge::record_path,corrupt);badge::ProfileSnapshot bad;assert(!badge::read_record(badge::record_path.c_str(),bad));}write_bytes(badge::record_path,unchanged);
 assert(badge::save_record(next,nullptr,0,true,error));{ConferenceProfileStore loaded;assert(loaded.begin());assert(loaded.profile().name==next.name.c_str());assert(!loaded.avatarPixels());}
 std::string name,url;assert(badge::name_valid("  Jos\xc3\xa9  ",name)&&name=="Jos\xc3\xa9");assert(!badge::name_valid("line\nbreak",name));assert(!badge::name_valid(std::string("a\0b",3),name));assert(badge::social_url(1,"https://twitter.com/example_1/",url)&&url=="https://x.com/example_1");assert(!badge::social_url(0,"https://github.com.evil/account",url));
 assert(badge::profile_initialize_for_conference()&&format_calls==0);
 badge::stored.ready=false;badge::portal.active=true;assert(!badge::profile_initialize_for_conference()&&format_calls==0);badge::portal.active=false;
 for(size_t i=0;i<partitions.size();++i){partitions[i].address++;assert(!badge::profile_initialize_for_conference()&&format_calls==0);partitions[i].address--;}
 partitions.push_back(partitions.back());assert(!badge::profile_initialize_for_conference()&&format_calls==0);partitions.pop_back();
 native_mountable=true;assert(badge::profile_initialize_for_conference()&&format_calls==0&&badge::stored.profile.name==next.name);
 badge::stored.ready=false;native_mountable=false;native_mounted=false;assert(badge::profile_initialize_for_conference()&&format_calls==1&&badge::stored.profile.name.empty());
 assert(badge::profile_initialize_for_conference()&&format_calls==1);
 std::filesystem::remove_all(root);puts("Native storage: legacy/native fields+avatar compatibility, corruption, sync/rename failures, image removal and explicit initialization guards passed");
}
