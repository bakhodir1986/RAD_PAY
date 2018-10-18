using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class admin_onlineViewModel
    {
        public long aid { get; set; }
        public string token { get; set; }
        public DateTime? login_ts { get; set; }
        public DateTime? last_ts { get; set; }
    }
}