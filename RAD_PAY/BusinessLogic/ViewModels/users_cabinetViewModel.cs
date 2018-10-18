using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class users_cabinetViewModel
    {
        public long id { get; set; }
        public long uid { get; set; }
        public string password { get; set; }
        public int? checkpassword { get; set; }
        public DateTime? last_passwd_check { get; set; }
        public int? check_count { get; set; }
        public DateTime? checkcode_expiry { get; set; }
    }
}