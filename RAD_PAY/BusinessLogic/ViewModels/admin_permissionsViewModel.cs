using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class admin_permissionsViewModel
    {
        public long aid { get; set; }
        public int? modules { get; set; }
        public int? merchant { get; set; }
        public long bank { get; set; }
        public int permit_flag { get; set; }
    }
}