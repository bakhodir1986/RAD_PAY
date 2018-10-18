using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class users_full_registersViewModel
    {
        public long id { get; set; }
        public long? uid { get; set; }
        public string fio { get; set; }
        public string passport_number { get; set; }
        public DateTime? passport_start_date { get; set; }
        public DateTime? passport_end_date { get; set; }
        public string passport_image_path { get; set; }
        public DateTime? date_of_birth { get; set; }
        public string nationality { get; set; }
        public string citizenship { get; set; }
        public int? status { get; set; }
        public string passport_serial { get; set; }
        public int? level { get; set; }
    }
}