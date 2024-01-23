//Copyright (C) 2024 D. Michael Agun
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License.

// Internal string functions

const char* _string_find_dq_special(const char *str, unsigned int len);
const char* _string_rfind_dq_special(const char *str, unsigned int len);
int _string_findi(const char *str,unsigned int len, const char *substr,unsigned int substrn);
int _string_rfindi(const char *str,unsigned int len, const char *substr,unsigned int substrn);
int _string_findi_whitespace(const char *str,unsigned int len);
int _string_findi_notwhitespace(const char *str,unsigned int len);
int _string_rfindi_notwhitespace(const char *str,unsigned int len);
int _string_findi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars);
int _string_rfindi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars);
int _string_findi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars);
int _string_rfindi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars);
