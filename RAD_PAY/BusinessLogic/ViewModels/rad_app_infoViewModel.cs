using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class rad_app_infoViewModel
    {
        public int id { get; set; }
        public string version { get; set; }
        public string os { get; set; }
        public DateTime? release_date { get; set; }
        public DateTime? expiry_date { get; set; }
        public string min_version { get; set; }
        public string a3 { get; set; }
    }
}